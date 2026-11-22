#include <math.h>
#include <stdint.h>
#include <string.h>
#include "host.h"
#include "dispatch.h"
#include "input.h"
#include "platform/platform.h"

// Slots belong to recomp/raiders_mouse.py. PlayerCamera is updated on
// a guest fiber. Match its Cpu, camera and stack before consuming a marker.
typedef struct {
    Cpu *cpu;
    uint64_t camera, sp;
} OrbitFrame;

static PlatMutex orbit_lock = PLAT_MUTEX_INIT;
static OrbitFrame manual, pitch_frame;
static float pitch_step, pitch_coordinate;
static int pitch_solved;

static OrbitFrame frame(Cpu *c) {
    return (OrbitFrame){c, c->x[19], c->sp};
}

static int matches(OrbitFrame f, Cpu *c) {
    return f.cpu == c && f.camera == c->x[19] && f.sp == c->sp;
}

static float field(uint64_t camera, unsigned offset) {
    float value;
    memcpy(&value, (const void *)(uintptr_t)(camera + offset), sizeof value);
    return value;
}

static int requested_view(uint64_t camera) {
    // Nonzero requests select a different pose builder.
    uint32_t request;
    memcpy(&request, (const void *)(uintptr_t)(camera + 0x1770), sizeof request);
    return request != 0;
}

static void accept_manual(Cpu *c) {
    // Earlier branches skip this instruction when control is suppressed.
    // The following FCSEL pair also suppresses input in modes 1 and 2.
    uint32_t mode = (uint32_t)c->x[8];
    plat_mutex_lock(&orbit_lock);
    manual = mode == 1 || mode == 2 || requested_view(c->x[19]) ? (OrbitFrame){0} : frame(c);
    plat_mutex_unlock(&orbit_lock);
}

static void orbit_step(Cpu *c) {
    uint64_t camera = c->x[19];
    plat_mutex_lock(&orbit_lock);
    int allowed = matches(manual, c);
    manual = (OrbitFrame){0};
    pitch_frame = (OrbitFrame){0};
    pitch_solved = 0;

    float delta[2];
    input_raiders_orbit(allowed ? camera : 0, delta);
    if (delta[0]) {
        float extra;
        memcpy(&extra, &c->v[4], sizeof extra);
        // s4 is the additive angle read at +0x1550. The guest adds the pad
        // term in s0, rotates its heading vector and clears the extra angle.
        extra -= remainderf(delta[0], 360.0f) * 0.01745329252f;
        memcpy(&c->v[4], &extra, sizeof extra);
    }
    if (delta[1]) {
        pitch_step = delta[1];
        pitch_frame = frame(c);
    }
    plat_mutex_unlock(&orbit_lock);
}

static float scalar(const Cpu *c, unsigned index) {
    float value;
    memcpy(&value, &c->v[index], sizeof value);
    return value;
}

static void set_scalar(Cpu *c, unsigned index, float value) {
    memcpy(&c->v[index], &value, sizeof value);
}

// The native leaf maps the input angle onto the orbit coordinate and returns
// limit overflow through x2. It uses only arguments, immediates and scratch
// registers. Evaluate it on a private Cpu while leaving the live call intact.
static float native_coordinate(GuestFn map, Cpu *trial, const Cpu *args, float angle) {
    float overflow;
    trial->x[2] = (uintptr_t)&overflow;
    for (unsigned i = 0; i < 4; i++) trial->v[i] = args->v[i];
    set_scalar(trial, 1, angle);
    map(trial);
    return scalar(trial, 0);
}

static int apply_pitch(Cpu *c) {
    uint64_t camera = c->x[19];
    float center = field(camera, 0x15c0);
    float lower = field(camera, 0x15bc);
    float upper = field(camera, 0x15c4);
    if (!(lower < center && center < upper)) return 0;
    // PlayerCamera's ordinary pose builder interpolates between these three
    // angles using the signed orbit coordinate.
    float current = field(camera, 0x178);
    float angle = center + current * (current < 0 ? center - lower : upper - center);
    float target = fmaxf(lower, fminf(upper, angle + pitch_step));
    float wanted = (target - center) / (target < center ? center - lower : upper - center);

    GuestFn map = rt_dispatch_lookup(rt_raiders_pitch_map());
    if (!map) return 0;
    float reference = scalar(c, 1);
    float offset = field(camera, 0x1544);
    float origin = reference - offset;
    // The regular, monotonic branch ends at -165 and 60 degrees. The game
    // handles the wrapped reference during recentering. Keep its offset cap.
    if (!(reference >= -165.0f && reference < 60.0f)) return 0;
    float lo = fmaxf(-165.0f, origin - 90.0f);
    float hi = fminf(nextafterf(60.0f, 0.0f), origin + 90.0f);
    if (!(lo <= reference && reference <= hi)) return 0;
    Cpu trial = *c;
    float before = native_coordinate(map, &trial, c, reference);
    // Search from the current angle toward the requested one. Stop at the
    // near edge of a saturated interval so reversing at a limit is immediate.
    int increasing = wanted >= before;
    if (increasing) lo = reference;
    else hi = reference;
    for (int i = 0; i < 24 && lo < hi; ++i) {
        float mid = lo + (hi - lo) * 0.5f;
        if (mid == lo || mid == hi) break;
        float coordinate = native_coordinate(map, &trial, c, mid);
        if (coordinate < wanted || (!increasing && coordinate == wanted)) lo = mid;
        else hi = mid;
    }
    float result = increasing ? hi : lo;
    float new_offset = result - origin;
    pitch_coordinate = native_coordinate(map, &trial, c, result);
    memcpy((void *)(uintptr_t)(camera + 0x1544), &new_offset, sizeof new_offset);
    set_scalar(c, 1, result);
    if (c->x[1] == 1) {
        set_scalar(c, 8, new_offset);   // the caller writes s8 + limit overflow back
    } else {
        set_scalar(c, 9, result);      // combined sensor reference and offset
        set_scalar(c, 11, new_offset);
    }
    return 1;
}

static void pitch_map(Cpu *c) {
    plat_mutex_lock(&orbit_lock);
    if (matches(pitch_frame, c) && !pitch_solved)
        pitch_solved = apply_pitch(c);
    plat_mutex_unlock(&orbit_lock);
}

static void pitch_response(Cpu *c) {
    plat_mutex_lock(&orbit_lock);
    if (pitch_solved && matches(pitch_frame, c) && !requested_view(c->x[19])) {
        // The next two instructions interpolate the normal orbit coordinate
        // from current to target. Use the target on a mouse pitch update.
        set_scalar(c, 0, pitch_coordinate);
        c->v[2] = 0x3f800000u;
        pitch_frame = (OrbitFrame){0};
        pitch_solved = 0;
    }
    plat_mutex_unlock(&orbit_lock);
}

void raiders_camera_init(void) {
    if (!rt_patch_enabled(rt_patch_set("raidersmouse"))) return;
    uint64_t page = (uint64_t)(uintptr_t)rt_patch_page();
    rt_register_host(page + 0x300, accept_manual);
    rt_register_host(page + 0x310, orbit_step);
    rt_register_host(page + 0x320, pitch_response);
    rt_register_host(page + 0x330, pitch_map);
}
