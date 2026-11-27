#include <math.h>
#include <string.h>
#include "cfg.h"
#include "host.h"
#include "input.h"
#include "input_map.h"
#include "input_wsys.h"
#include "nvn_api.h"
#include "platform/platform.h"

// The raw path has its own lock. A camera update never waits on SDL pad polling.
static PlatMutex sample_lock = PLAT_MUTEX_INIT;
static uint64_t orbit_camera, orbit_present;
static int orbit_cursor;
static int zl_held;

int input_raiders_available(void) {
    return rt_patch_enabled(rt_patch_set("raidersmouse"));
}

static uint64_t present_count(void) {
    return __atomic_load_n(&gfx_counters.frames, __ATOMIC_RELAXED);
}

static int set_orbit_cursor(int active) {
    return __atomic_exchange_n(&orbit_cursor, active, __ATOMIC_RELAXED) != active;
}

static int orbit_expired(void) {
    // Native pause screens can stop player updates while HID and presentation
    // continue. This watchdog releases ownership. It never scales motion.
    return orbit_camera && present_count() - orbit_present > 4;
}

int input_raiders_owns_cursor(void) {
    return __atomic_load_n(&orbit_cursor, __ATOMIC_RELAXED);
}

void input_raiders_cursor_tick(void) {
    if (!input_raiders_owns_cursor()) return;
    plat_mutex_lock(&sample_lock);
    int changed = 0;
    if (orbit_expired()) {
        orbit_camera = 0;
        changed = set_orbit_cursor(0);
    }
    plat_mutex_unlock(&sample_lock);
    if (changed) ikb_cursor_update();
}

void input_raiders_buttons(uint64_t buttons) {
    __atomic_store_n(&zl_held, (buttons & (1ull << B_ZL)) != 0, __ATOMIC_RELAXED);
}

int input_raiders_orbit(uint64_t camera, float degrees[2]) {
    int32_t dx, dy;
    plat_mutex_lock(&sample_lock);
    double sens = cfg_double("TOTK_MOUSE_SENS");
    float scale = (float)(fmin(sens, 10.0) * 0.064);
    const char *in = cfg_get("TOTK_INPUT");
    if (!(scale > 0) || !isfinite(sens) || (in && !strcmp(in, "none")))
        camera = 0;
    int continuous = camera && orbit_camera == camera && !orbit_expired();
    orbit_camera = camera;
    orbit_present = present_count();
    int changed = set_orbit_cursor(camera != 0);
    ikb_mouse_take(&dx, &dy);
    int active = continuous && !input_blocks_mouse() && ikb_foreground() && ikb_captured();
    int swim = __atomic_load_n(&zl_held, __ATOMIC_RELAXED);
    degrees[0] = active ? dx * scale : 0;
    degrees[1] = active ? dy * (cfg_bool("TOTK_MOUSE_INVERT_Y") ? scale : -scale) : 0;
    plat_mutex_unlock(&sample_lock);
    if (changed) ikb_cursor_update();
    return swim;
}
