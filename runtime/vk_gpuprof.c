// GPU time per shader pair (DKB_GPU_PROFILE=1, a diagnostic): after every draw and dispatch the replay writes a GPU
// timestamp (vk_gpuprof_note, from vk_exec_draw.c note_work). The time between two consecutive timestamps of one
// command buffer is charged to the draw or dispatch that ends the interval, keyed by the NVN 2 names of its stages
// (vertex/fragment, or compute/0). Copies, clears and barriers between two draws are charged to the later draw.
// Every 10 seconds one log block lists the pairs that took the most GPU time per frame. The pool is a ring of
// timestamp queries reset on the host after their read (hostQueryReset, Vulkan 1.2); a full ring skips samples.
#include <stdlib.h>
#include <string.h>

#include "host.h"
#include "nvn_api.h"
#include "vk.h"
#include "vk_exec.h"
#include "platform/platform.h"

#define GP_N 65536u   // timestamps in the ring
#define GP_KEYS 4096  // distinct shader pairs kept per report (open addressing)
#define GP_TOP 45

typedef struct { uint64_t a, b; } GpKey;
typedef struct { uint64_t a, b, ns, n; } GpSum;
typedef struct { uint32_t first, count; } GpBatch;

static int gp_state;                        // 0 not tried, 1 on, -1 off
static VkQueryPool gp_pool;
static double gp_period;                    // ns per tick
static GpKey *gp_key;                       // per query: the pair whose work ended there (a = b = 0: a start mark)
static uint32_t gp_head;                    // replay thread: next query to write (counter, modulo GP_N)
static volatile uint32_t gp_tail;           // completion thread: queries before it were read and reset
static VkCommandBuffer gp_cb;               // replay thread: the command buffer of the latest timestamp
static uint64_t gp_fence;                   // replay thread: the submission of the open batch
static uint32_t gp_batch_first;
static GpSum gp_sum[GP_KEYS];               // completion thread
static uint64_t gp_t0, gp_frames0;

static int gp_on(void) {
    if (gp_state) return gp_state > 0;
    gp_state = -1;
    const char *e = getenv("DKB_GPU_PROFILE");
    if (!e || !*e || !strcmp(e, "0")) return 0;
    VkCtx *c = vk_ctx();
    VkPhysicalDeviceVulkan12Features f12 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceFeatures2 f2 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &f12};
    vkGetPhysicalDeviceFeatures2(c->phys, &f2);
    float period = c->props.limits.timestampPeriod;
    VkQueryPoolCreateInfo qi = {.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, .queryType = VK_QUERY_TYPE_TIMESTAMP,
                                .queryCount = GP_N};
    if (!f12.hostQueryReset || period <= 0 || vkCreateQueryPool(c->device, &qi, NULL, &gp_pool) != VK_SUCCESS) {
        rt_log("gpuprof: off (no hostQueryReset, no timestamps or no query pool)");
        return 0;
    }
    gp_key = calloc(GP_N, sizeof *gp_key);
    if (!gp_key) return 0;
    vkResetQueryPool(c->device, gp_pool, 0, GP_N);
    gp_period = period;
    gp_state = 1;
    rt_log("gpuprof: on: GPU time per shader pair every 10 s (DKB_GPU_PROFILE)");
    return 1;
}

static void gp_add(GpKey k, uint64_t ns) {
    uint64_t h = (k.a * 0x9E3779B97F4A7C15ull) ^ (k.b * 0xC2B2AE3D27D4EB4Full);
    for (uint32_t i = 0; i < GP_KEYS; i++) {
        GpSum *s = &gp_sum[(h + i) % GP_KEYS];
        if (!s->n && !s->ns) s->a = k.a, s->b = k.b;
        if (s->a == k.a && s->b == k.b) {
            s->ns += ns, s->n++;
            return;
        }
    }
}

static int by_ns(const void *x, const void *y) {
    const GpSum *a = x, *b = y;
    return a->ns < b->ns ? 1 : a->ns > b->ns ? -1 : 0;
}

static void gp_report(void) {
    uint64_t now = kern_now_ns(), frames = perf_frames_presented();
    if (!gp_t0) {
        gp_t0 = now, gp_frames0 = frames;
        return;
    }
    if (now - gp_t0 < 10000000000ull) return;
    double nf = frames > gp_frames0 ? (double)(frames - gp_frames0) : 1;
    static GpSum s[GP_KEYS];
    memcpy(s, gp_sum, sizeof s);
    memset(gp_sum, 0, sizeof gp_sum);
    qsort(s, GP_KEYS, sizeof *s, by_ns);
    uint64_t total = 0;
    for (int i = 0; i < GP_KEYS; i++) total += s[i].ns;
    rt_log("gpuprof: %.0f frames in %.1f s: %.2f ms of GPU time per frame measured between draws; the costliest pairs:", nf,
           (now - gp_t0) / 1e9, total / 1e6 / nf);
    for (int i = 0; i < GP_TOP && s[i].ns; i++)
        if ((s[i].a >> 48) == 0xBA55)
            rt_log("gpuprof:   %s #%-4u %5ux%-5u %016llx %.2f ms per frame (%.1f%%), %.1f per frame",
                   (uint16_t)(s[i].a >> 16) ? "pass" : "disp", (unsigned)(s[i].a >> 32 & 0xffff),
                   (unsigned)(s[i].a >> 16 & 0xffff), (unsigned)(s[i].a & 0xffff), (unsigned long long)s[i].b,
                   s[i].ns / 1e6 / nf, total ? 100.0 * s[i].ns / total : 0, s[i].n / nf);
        else if ((s[i].a >> 48) == 0xC0DE)
            rt_log("gpuprof:   op %-26s %.2f ms per frame (%.1f%%), %.1f calls per frame", (uint16_t)s[i].a == 0xFFF0 ? "(command buffer start)" : (uint16_t)s[i].a == 0xFFF1 ? "(mirror write-back, end)" : (uint16_t)s[i].a == 0xFFF2 ? "(last pass end)" : (uint16_t)s[i].a == 0xFFF3 ? "(final barrier)" : (uint16_t)s[i].a == 0xFFF4 ? "(between command buffers)" : (uint16_t)s[i].a == 0xFFF5 ? "(between submissions)" : nvn_op_name((uint32_t)s[i].a),
                   s[i].ns / 1e6 / nf, total ? 100.0 * s[i].ns / total : 0, s[i].n / nf);
        else
            rt_log("gpuprof:   %016llx/%016llx %.2f ms per frame (%.1f%%), %.1f calls per frame", (unsigned long long)s[i].a,
                   (unsigned long long)s[i].b, s[i].ns / 1e6 / nf, total ? 100.0 * s[i].ns / total : 0, s[i].n / nf);
    gp_t0 = now, gp_frames0 = frames;
}

static void gp_read(void *arg) {   // completion thread, in submission order
    GpBatch *b = arg;
    VkCtx *c = vk_ctx();
    // The GPU clock of the last timestamp read, across batches: the time from a command buffer's last timestamp to the
    // start mark of the next one is charged to "(between command buffers)" within a submission and "(between
    // submissions)" across them, the GPU waiting for the replay or for a semaphore rather than working.
    static uint64_t last_t;
    uint64_t prev = 0;
    int have = 0;
    for (uint32_t k = 0; k < b->count; k++) {
        uint32_t q = (b->first + k) % GP_N;
        uint64_t t[2] = {0, 0};   // value, availability
        VkResult r = vkGetQueryPoolResults(c->device, gp_pool, q, 1, sizeof t, t, sizeof t,
                                           VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
        GpKey key = gp_key[q];
        vkResetQueryPool(c->device, gp_pool, q, 1);
        if (r != VK_SUCCESS || !t[1]) { have = 0; continue; }
        if (key.a || key.b) {
            if (have && t[0] >= prev) gp_add(key, (uint64_t)((double)(t[0] - prev) * gp_period));
        } else if (last_t && t[0] >= last_t && t[0] - last_t < (uint64_t)(1e9 / gp_period)) {
            gp_add((GpKey){0xC0DE000000000000ull | (k ? 0xFFF4 : 0xFFF5), 0}, (uint64_t)((double)(t[0] - last_t) * gp_period));
        }
        prev = t[0], have = 1, last_t = t[0];
        if (!key.a && !key.b) have = 1;   // a start mark: only opens the interval
    }
    __atomic_store_n(&gp_tail, b->first + b->count, __ATOMIC_RELEASE);
    free(b);
    gp_report();
}

static void gp_close(void) {   // replay thread: the open batch goes to its submission's completion
    uint32_t n = gp_head - gp_batch_first;
    if (!n) return;
    GpBatch *b = malloc(sizeof *b);
    if (!b) return;
    b->first = gp_batch_first, b->count = n;
    vk_completion_add(gp_fence, gp_read, b);
    gp_batch_first = gp_head;
}

static int gp_write(VkCommandBuffer cb, GpKey k) {
    if (gp_head - __atomic_load_n(&gp_tail, __ATOMIC_ACQUIRE) >= GP_N) return 0;   // ring full
    uint32_t q = gp_head % GP_N;
    gp_key[q] = k;
    vkCmdWriteTimestamp2(cb, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, gp_pool, q);
    gp_head++;
    return 1;
}

void vk_gpuprof_note(VkCommandBuffer cb, uint64_t fence, uint64_t a, uint64_t b) {
    if (!gp_on()) return;
    if (fence != gp_fence) {
        gp_close();
        gp_fence = fence;
        gp_cb = VK_NULL_HANDLE;
    }
    if (cb != gp_cb) {   // a new command buffer: a start mark, so its first draw is measured from here
        if (!gp_write(cb, (GpKey){0, 0})) return;
        gp_cb = cb;
    }
    if (!a && !b) a = 1;   // a program without NVN 2 names still counts (as 0000...0001)
    if (!gp_write(cb, (GpKey){a, b})) gp_cb = VK_NULL_HANDLE;
}
