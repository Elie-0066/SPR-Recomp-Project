/* A synthetic orbit and window exercise the real hooks and lifted pitch map. */
#include "../runtime/raiders_mouse.c"
#include "../runtime/raiders_camera.c"
#include <assert.h>

static int captured, foreground = 1, inverse;
static float sensitivity = 1;
static int g_menu, g_block;
static int32_t raw_x, raw_y;
static int neutral_reference = 1;
static float posture, reference_origin = -75;
static uint8_t object[0x1858] __attribute__((aligned(16)));
static Cpu cpu;
static GuestFn sites[4];
volatile int g_fnprof;
void fnprof_hit(uint64_t a, uint64_t b, uint64_t d) { abort(); }
void rt_udf(Cpu *c, uint64_t pc) { abort(); }
extern void f_7002466510(Cpu *c);
uint64_t rt_raiders_pitch_map(void) { return 0x7002467510ull; }
GuestFn rt_dispatch_lookup(uint64_t pc) { assert(pc == rt_raiders_pitch_map()); return f_7002466510; }
GfxStats gfx_counters;

void plat_mutex_lock(PlatMutex *m) { assert(!m->p); m->p = m; }
void plat_mutex_unlock(PlatMutex *m) { assert(m->p); m->p = NULL; }
void *rt_patch_page(void) { return (void *)(uintptr_t)0x7004e30000ull; }
int rt_patch_set(const char *name) { return !strcmp(name, "raidersmouse") ? 0 : -1; }
int rt_patch_enabled(int id) { return id == 0; }
void rt_register_host(uint64_t addr, GuestFn f) {
    uint64_t slot = addr - (uint64_t)(uintptr_t)rt_patch_page() - 0x300;
    assert(slot <= 0x30 && slot % 16 == 0);
    sites[slot / 16] = f;
}
int input_blocks_mouse(void) { return g_menu || g_block; }
double cfg_double(const char *key) { assert(!strcmp(key, "TOTK_MOUSE_SENS")); return sensitivity; }
int cfg_bool(const char *key) { assert(!strcmp(key, "TOTK_MOUSE_INVERT_Y")); return inverse; }
const char *cfg_get(const char *key) { assert(!strcmp(key, "TOTK_INPUT")); return NULL; }
int ikb_foreground(void) { return foreground; }
int ikb_captured(void) { return captured; }
void ikb_cursor_update(void) { captured = foreground && input_raiders_owns_cursor() && !input_blocks_mouse(); }
void ikb_mouse_take(int32_t *x, int32_t *y) {
    *x = raw_x; *y = raw_y; raw_x = raw_y = 0;
}

static void put_field(unsigned off, float value) { memcpy(object + off, &value, sizeof value); }
static float view_degrees(void) {
    float current = field(cpu.x[19], 0x178), center = field(cpu.x[19], 0x15c0);
    float range = current < 0 ? center - field(cpu.x[19], 0x15bc) : field(cpu.x[19], 0x15c4) - center;
    return center + current * range;
}

static float update(int mode, int x, int y) {
    raw_x = x; raw_y = y;
    cpu.x[8] = mode;
    cpu.v[4] = 0;
    cpu.v[2] = 0x3e4ccccd;
    if (mode >= 0) sites[0](&cpu);
    sites[1](&cpu);
    float radians;
    memcpy(&radians, &cpu.v[4], 4);
    float offset = field(cpu.x[19], 0x1544), overflow;
    cpu.x[0] = 1;
    cpu.x[1] = neutral_reference;
    cpu.x[2] = (uintptr_t)&overflow;
    set_scalar(&cpu, 0, posture);
    set_scalar(&cpu, 1, reference_origin + offset);
    set_scalar(&cpu, 2, 0);
    set_scalar(&cpu, 3, 0);
    set_scalar(&cpu, 8, offset);
    set_scalar(&cpu, 11, offset);
    sites[3](&cpu);
    f_7002466510(&cpu);
    if (neutral_reference) put_field(0x1544, scalar(&cpu, 8) + overflow);
    set_scalar(&cpu, 2, 0.2f);
    sites[2](&cpu);
    float current = field(cpu.x[19], 0x178);
    put_field(0x178, current + (scalar(&cpu, 0) - current) * scalar(&cpu, 2));
    return radians * 57.29577951f;
}

static void near(float actual, float expected) {
    if (!isfinite(actual) || fabsf(actual - expected) > 0.0001f) {
        fprintf(stderr, "expected %.7f, got %.7f\n", expected, actual);
        abort();
    }
}

int main(void) {
    raiders_camera_init();
    assert(sites[0] && sites[1] && sites[2] && sites[3]);
    cpu.x[19] = (uintptr_t)object;
    cpu.sp = 0x12340;
    put_field(0x15bc, -75);
    put_field(0x15c0, -7.5f);
    put_field(0x15c4, 60);
    near(update(0, 999, 999), 0);
    assert(captured);
    near(update(0, 100, -50), -6.4f);
    near(view_degrees(), -7.5f + 3.2f);
    assert(cpu.v[2] == 0x3f800000);
    near(update(0, -100, 50), 6.4f);
    near(view_degrees(), -7.5f);
    near(update(0, 0, 0), 0);
    assert(cpu.v[2] == 0x3e4ccccd);
    float sum = 0;
    for (int i = 0; i < 8; i++) {
        gfx_counters.frames += i % 3;
        sum += update(0, 300, 0);
    }
    near(sum, update(0, 2400, 0));
    inverse = 1; sensitivity = 2;
    near(update(0, 50, 25), -6.4f);
    near(view_degrees(), -7.5f + 3.2f);
    for (int mode = -1; mode <= 2; mode += (mode == -1 ? 2 : 1)) {
        near(update(mode, 400, 400), 0);
        assert(!captured);
        near(update(0, 400, 400), 0);
    }
    *(uint32_t *)(object + 0x1770) = 3;
    near(update(0, 100, 100), 0);
    *(uint32_t *)(object + 0x1770) = 0;
    near(update(0, 100, 100), 0);
    g_menu = 1; near(update(0, 500, 100), 0); g_menu = 0;
    g_block = 1; near(update(0, 500, 100), 0); g_block = 0;
    foreground = 0; near(update(0, 500, 100), 0); foreground = 1;
    gfx_counters.frames += 8; near(update(0, 999, 999), 0);
    assert(!raw_x && !raw_y);
    near(view_degrees(), -7.5f + 3.2f);
    update(0, 0, 2000); near(view_degrees(), 60);
    update(0, 0, -10); near(view_degrees(), 60 - 1.28f);
    update(0, 0, -2000); near(view_degrees(), -75);
    update(0, 0, 10); near(view_degrees(), -75 + 1.28f);
    put_field(0x15bc, -50);
    put_field(0x15c0, 0);
    put_field(0x15c4, 40);
    float start = view_degrees();
    update(0, 0, 500); near(view_degrees(), start + 64);
    update(0, 0, -500); near(view_degrees(), start);
    neutral_reference = 0;
    reference_origin = -72;
    posture = 0.5f;
    update(0, 0, 50); near(view_degrees(), start + 6.4f);
    update(0, 0, -50); near(view_degrees(), start);
    sensitivity = 0;
    near(update(0, 500, 100), 0); assert(!captured);
    puts("PASS: raw distance, reversals, split batches, native pitch map and limits, changed pose, sensor reference, inversion, mode gates, focus, menus and cursor");
    return 0;
}
