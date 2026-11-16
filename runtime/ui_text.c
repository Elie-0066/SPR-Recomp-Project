// Catalog of the text the host shows the player (ui_text.h). One array per catalog language, indexed by UiTextId: a new
// string gets an id in ui_text.h and an entry in every language that has one (a missing entry falls back to English).
// The software keyboard has every language; the menu has English, Russian and Korean.
#include <stdatomic.h>
#include <string.h>

#include "cfg.h"
#include "ov.h"
#include "platform/platform.h"
#include "ui_text.h"

typedef struct { const char *lang; const char *const *text; } Row;

// The language names are the same in every row.
#define LANG_NAMES \
    [UI_MENU_LANGNAME_EN] = "English", [UI_MENU_LANGNAME_JA] = "日本語", [UI_MENU_LANGNAME_FR] = "Français", \
    [UI_MENU_LANGNAME_DE] = "Deutsch", [UI_MENU_LANGNAME_IT] = "Italiano", [UI_MENU_LANGNAME_ES] = "Español", \
    [UI_MENU_LANGNAME_NL] = "Nederlands", [UI_MENU_LANGNAME_RU] = "Русский", \
    [UI_MENU_LANGNAME_ZH_HANS] = "简体中文", [UI_MENU_LANGNAME_ZH_HANT] = "繁體中文", [UI_MENU_LANGNAME_KO] = "한국어"

// SWKBD: TITLE, PROMPT, DEFAULT_NAME (at most 9 UTF-16 units, no '@' '%' '\'), OK, CANCEL, PAD_HINT.
#define SWKBD(title, prompt, name, ok, cancel, hint) \
    [UI_SWKBD_TITLE] = title, [UI_SWKBD_PROMPT] = prompt, [UI_SWKBD_DEFAULT_NAME] = name, [UI_SWKBD_OK] = ok, \
    [UI_SWKBD_CANCEL] = cancel, [UI_SWKBD_PAD_HINT] = hint, LANG_NAMES

static const char *const text_en[UI_TEXT_COUNT] = {
    SWKBD("Text entry", "Enter a name", "Horse", "OK", "Cancel", "Controller: A = OK, B = Cancel"),

    [UI_MENU_TITLE] = "Settings",
    [UI_MENU_TAB_SCREEN] = "Screen", [UI_MENU_TAB_GAME] = "Game", [UI_MENU_TAB_SOUND] = "Sound",
    [UI_MENU_TAB_CONTROLS] = "Controls", [UI_MENU_TAB_MENU] = "Menu", [UI_MENU_TAB_ADVANCED] = "Advanced",
    [UI_MENU_CONTINUE] = "Continue", [UI_MENU_QUIT] = "Quit game",
    [UI_MENU_ON] = "On", [UI_MENU_OFF] = "Off", [UI_MENU_AUTO] = "Automatic", [UI_MENU_DEFAULT_MARK] = "default",
    [UI_MENU_AFTER_RESTART] = "after restart", [UI_MENU_SET_BY_ENV] = "set by environment variable",
    [UI_MENU_SAVE_FAILED] = "could not save settings",
    [UI_MENU_VERSION] = "DKB Recomp %s, game %s",

    [UI_MENU_FULLSCREEN] = "Fullscreen", [UI_MENU_FULLSCREEN_DESC] = "Fills the screen with the game; F11 toggles it.",
    [UI_MENU_WINDOW_MODE] = "Window mode",
    [UI_MENU_WINDOW_MODE_DESC] = "Windowed, borderless over the screen, or fullscreen; F11 switches between windowed and the last full-screen mode.",
    [UI_MENU_WINDOW_MODE_WINDOWED] = "Windowed", [UI_MENU_WINDOW_MODE_BORDERLESS] = "Borderless",
    [UI_MENU_WINDOW_MODE_FULLSCREEN] = "Fullscreen",
    [UI_MENU_WINDOW_SIZE] = "Window size", [UI_MENU_WINDOW_SIZE_DESC] = "Size of the game window when not fullscreen.",
    [UI_MENU_WINDOW_SIZE_CUSTOM] = "Custom",
    [UI_MENU_WINDOWED_ONLY] = "Only available in windowed mode",
    [UI_MENU_PRESENT] = "Presentation", [UI_MENU_PRESENT_DESC] = "How finished frames reach the screen.",
    [UI_MENU_PRESENT_MAILBOX] = "Mailbox", [UI_MENU_PRESENT_VSYNC] = "VSync", [UI_MENU_PRESENT_IMMEDIATE] = "Immediate",
    [UI_MENU_PRESENT_IN_USE] = "in use: %s",
    [UI_MENU_GPU] = "Graphics card", [UI_MENU_GPU_DESC] = "The Vulkan device that draws the game.",
    [UI_MENU_GPU_AUTO] = "Automatic",
    [UI_MENU_PERF_OVERLAY] = "Performance overlay",
    [UI_MENU_PERF_OVERLAY_DESC] = "Frame rate in the top right corner. Full adds frame times with a graph, CPU, GPU and "
                                  "memory load, temperatures and the render thread's waits. F10 cycles the modes.",
    [UI_MENU_PERF_OFF] = "Off", [UI_MENU_PERF_FPS] = "FPS", [UI_MENU_PERF_FULL] = "Full",
    // printf format of the overlay line in FPS mode: the frame rate
    [UI_PERF_LINE_FPS] = "%.0f fps",
    [UI_MENU_GRAPHICS] = "Picture",
    [UI_MENU_OUTPUT_RES] = "Render resolution",
    [UI_MENU_ULTRAWIDE] = "Ultrawide",
    [UI_MENU_ULTRAWIDE_DESC] = "Expand the camera's horizontal view for 21:9 or 32:9. Menus keep their proportions. "
                              "Off uses a 16:9 view. Changing this selects a resolution with the nearest height. Restart to apply.",
    [UI_MENU_WIDE_RES] = "Render resolution",
    [UI_MENU_WIDE_RES_DESC] = "The resolution the game renders before scaling the image to your window or screen. "
                             "Higher resolutions improve detail but need more GPU time and video memory. "
                             "Ultrawide enables the 21:9 and 32:9 choices. Restart to apply.",
    [UI_MENU_HUD_POSITION] = "HUD position",
    [UI_MENU_HUD_POSITION_DESC] = "Place corner displays near the screen edges, or keep them inside a centered 16:9 "
                                 "area. Menus and aiming markers stay centered. Restart to apply.",
    [UI_MENU_HUD_EDGES] = "Screen edges", [UI_MENU_HUD_CENTERED] = "Centered (16:9)",
    [UI_MENU_WIDE_DISABLED] = "Enable Ultrawide first",
    [UI_MENU_OUTPUT_RES_DESC] = "The size the game draws its picture and interface in. Above 1920 x 1080 the game needs "
                                "more video memory and GPU time. A wider size (21:9) widens the camera's view and moves "
                                "the HUD to the screen's corners; a taller one (16:10, Steam Deck) moves it to the top "
                                "and bottom edges.",
    [UI_MENU_RENDER_SCALE] = "3D render scale",
    [UI_MENU_RENDER_SCALE_DESC] = "Size of the 3D scene relative to the game resolution; the game's FSR scales it to "
                                  "that size. Above 100 % smooths jagged edges (supersampling) at a high GPU cost.",
    [UI_MENU_RENDER_SCALE_GAME] = "Game (83 %)",
    [UI_MENU_RAIDERS_RES] = "Resolution",
    [UI_MENU_RAIDERS_RES_DESC] = "The output the game renders for, as a Switch 2 on a TV of that resolution: docked "
                                 "1920 x 1080, 2560 x 1440 or 3840 x 2160; handheld 1280 x 720 at 1080p and "
                                 "1920 x 1080 above. Higher needs more video memory and GPU time.",
    [UI_MENU_UPSCALER] = "Scaling to the window",
    [UI_MENU_UPSCALER_DESC] = "How the picture is scaled when the window is larger than the game resolution. FSR keeps "
                              "edges and detail sharper than bilinear.",
    [UI_MENU_UPSCALER_FSR] = "AMD FSR 1", [UI_MENU_UPSCALER_BILINEAR] = "Bilinear",
    [UI_MENU_ASPECT] = "Fit picture to window",
    [UI_MENU_ASPECT_DESC] = "When the window's shape differs from the game resolution's (a 16:9 picture on a 21:9 "
                            "screen): keep the picture's shape with black bars, or stretch it over the window. "
                            "This does not expand the camera's view.",
    [UI_MENU_ASPECT_FIT] = "Black bars", [UI_MENU_ASPECT_STRETCH] = "Stretch",
    [UI_MENU_FOV] = "Field of view",
    [UI_MENU_FOV_DESC] = "How much of the world the camera shows, in percent of the game's own. Game version 1.4.3 only.",
    [UI_MENU_SHARPNESS] = "FSR sharpness",
    [UI_MENU_SHARPNESS_DESC] = "Sharpening after FSR scaling; 0 turns it off.",
    [UI_MENU_SMAA] = "Anti-aliasing (SMAA)",
    [UI_MENU_SMAA_DESC] = "Smooths jagged edges on the final picture, after it is scaled to the window and before this "
                          "menu is drawn. The HUD is smoothed too. Higher quality costs more GPU time.",
    [UI_MENU_SMAA_LOW] = "Low", [UI_MENU_SMAA_MEDIUM] = "Medium", [UI_MENU_SMAA_HIGH] = "High",
    [UI_MENU_COLOR] = "Colour",
    [UI_MENU_COLOR_PRESET] = "Colour profile",
    [UI_MENU_COLOR_PRESET_DESC] = "Sets the four values below. Vivid: more contrast and saturation, shadows opened a "
                                  "little (made for OLED screens). Clarity: a little more contrast, saturation and "
                                  "brightness. Moving a value makes the profile Custom. The HUD changes with the picture; "
                                  "this menu does not.",
    [UI_MENU_COLOR_DEFAULT] = "Game's own", [UI_MENU_COLOR_VIVID] = "Vivid (OLED)", [UI_MENU_COLOR_CLARITY] = "Clarity",
    [UI_MENU_COLOR_CUSTOM] = "Custom",
    [UI_MENU_COLOR_BRIGHTNESS] = "Brightness", [UI_MENU_COLOR_BRIGHTNESS_DESC] = "Added to every colour; 0 leaves it.",
    [UI_MENU_COLOR_CONTRAST] = "Contrast",
    [UI_MENU_COLOR_CONTRAST_DESC] = "Spreads colours away from mid-grey (above 100 %) or towards it; 100 % leaves them.",
    [UI_MENU_COLOR_SATURATION] = "Saturation",
    [UI_MENU_COLOR_SATURATION_DESC] = "Colour strength against grey; 100 % leaves it.",
    [UI_MENU_COLOR_GAMMA] = "Gamma",
    [UI_MENU_COLOR_GAMMA_DESC] = "Below 1 brightens dark and middle tones, above 1 darkens them; 1 leaves them.",
    [UI_MENU_HDR_GROUP] = "HDR (experimental)",
    [UI_MENU_HDR] = "HDR output",
    [UI_MENU_HDR_DESC] = "Experimental: the game's SDR picture with its highlights lifted on an HDR display. Needs HDR turned on in Windows (\"Use HDR\") or in a "
                         "Wayland compositor with colour management; without it the picture stays SDR. Applies after a "
                         "restart.",
    [UI_MENU_HDR_NATIVE] = "Game's own HDR",
    [UI_MENU_HDR_NATIVE_DESC] = "With HDR output on: the game renders its own HDR picture, as on the console, with "
                                "highlights up to Peak brightness (the game stops at 1400 nits), instead of the lifted "
                                "SDR one. FSR, SMAA and the colour adjustment are off in this mode. Applies after a "
                                "restart.",
    [UI_MENU_HDR_PEAK] = "Peak brightness",
    [UI_MENU_HDR_PEAK_DESC] = "Your display's peak in nits; the brightest highlights stop there.",
    [UI_MENU_HDR_GAME] = "Game brightness",
    [UI_MENU_HDR_GAME_DESC] = "Nits of the picture's white (paper white). 203 is the HDR reference; raise it in a "
                              "bright room.",
    [UI_MENU_HDR_UI] = "Menu brightness",
    [UI_MENU_HDR_UI_DESC] = "Nits of this menu's white. The game's HUD is part of the picture and follows Game brightness.",

    [UI_MENU_FPS_LIMIT] = "Frame rate limit",
    [UI_MENU_DKB_FPS_LIMIT_DESC] = "With Game speed follows the clock enabled, changes apply immediately. Above 60 "
                                 "needs that experimental setting and enough CPU and GPU performance. Unlimited is experimental.",
    [UI_MENU_FPS_LIMIT_DESC] = "The most frames per second the game draws. Above 60 is experimental and needs Game speed "
                               "follows the clock. Unlimited is experimental.",
    [UI_MENU_FPS_30] = "30 FPS", [UI_MENU_FPS_40] = "40 FPS", [UI_MENU_FPS_45] = "45 FPS",
    [UI_MENU_FPS_60] = "60 FPS", [UI_MENU_FPS_90] = "90 FPS", [UI_MENU_FPS_120] = "120 FPS", [UI_MENU_FPS_144] = "144 FPS",
    // the monitor's refresh rate; printf format with the rate in Hz when it is known
    [UI_MENU_FPS_DISPLAY] = "Monitor rate", [UI_MENU_FPS_DISPLAY_HZ] = "Monitor rate (%.0f Hz)",
    [UI_MENU_FPS_UNLIMITED] = "Unlimited",
    [UI_MENU_FPS_NEEDS_REALTIME] = "Needs Game speed follows the clock (restart): limits below and above 60 FPS",
    [UI_MENU_REALTIME] = "Game speed follows the clock",
    [UI_MENU_REALTIME_DESC] = "Steps the game by real time, so a slow frame does not slow the game down.",
    [UI_MENU_DKB_REALTIME_DESC] = "Experimental; off by default. Adjusts game timing to measured frame duration. "
                                "Camera, physics, cutscenes and audio still need validation. Requires restarting.",
    [UI_MENU_CLOTH_REALTIME] = "Cloth on the clock",
    [UI_MENU_CLOTH_REALTIME_DESC] = "Steps cloth and hair by real time as well.",
    [UI_MENU_HIGHFPS_FIX] = "High frame rate fixes",
    [UI_MENU_HIGHFPS_FIX_DESC] = "Corrects physics that misbehave above 30 frames per second, and objects that stay see-through or invisible above 60.",
    [UI_MENU_GAME_LANG] = "Game language", [UI_MENU_GAME_LANG_DESC] = "Language of the game's text and voices.",
    [UI_MENU_GAME_LANG_AUTO] = "Automatic",
    [UI_MENU_OPERATION_MODE] = "Console mode",
    [UI_MENU_OPERATION_MODE_DESC] = "Whether the game thinks it runs docked or handheld.",
    [UI_MENU_MODE_DOCKED] = "Docked", [UI_MENU_MODE_HANDHELD] = "Handheld",
    [UI_MENU_ASSUME_FOCUS] = "Keep running out of focus",
    [UI_MENU_ASSUME_FOCUS_DESC] = "On (the default), the game keeps running while you are in another window, and the "
                                  "keyboard stays quiet until the window is in front again. Off, it pauses when its "
                                  "window loses focus, as on the Switch when Home is pressed; a load interrupted that "
                                  "way can stop for good.",

    [UI_MENU_VOLUME] = "Volume", [UI_MENU_VOLUME_DESC] = "Master volume of the game.",
    [UI_MENU_AUDIO_CHANNELS] = "Speakers",
    [UI_MENU_AUDIO_CHANNELS_DESC] = "Stereo or 5.1 surround output. Automatic picks 5.1 when the default output device "
                                    "has six or more channels. Applies after a restart.",
    [UI_MENU_CHANNELS_AUTO] = "Automatic", [UI_MENU_CHANNELS_STEREO] = "Stereo", [UI_MENU_CHANNELS_51] = "5.1 surround",

    [UI_MENU_MOTION] = "Motion controls", [UI_MENU_MOTION_DESC] = "Aiming with the controller's gyroscope.",
    [UI_MENU_RUMBLE_STRENGTH] = "Vibration strength",
    [UI_MENU_RUMBLE_STRENGTH_DESC] = "How strong the controller vibrates.",
    [UI_MENU_RUMBLE_CURVE] = "Vibration curve",
    [UI_MENU_RUMBLE_CURVE_DESC] = "Lower values make weak vibration stronger.",
    [UI_MENU_RUMBLE_TRIGGERS] = "Trigger vibration (Xbox)",
    [UI_MENU_RUMBLE_TRIGGERS_DESC] = "Vibrates the triggers of an Xbox controller too.",
    [UI_MENU_PAD_LAYOUT] = "Button layout",
    [UI_MENU_PAD_LAYOUT_DESC] = "Which button is A. Nintendo: the right one, as on a Switch. Xbox: the bottom one, as "
                                "labelled on an Xbox controller (cross on a PlayStation controller). Automatic picks "
                                "Nintendo for controllers with Nintendo labels and Xbox for the others. Xbox prompts: "
                                "A is the right one as with Nintendo, and the game shows the Xbox labels (B for A, A "
                                "for B, Y for X, X for Y); the prompts change after a restart. The letters are part of the "
                                 "game's files, so a button you remap in Controls keeps the old letter on screen.",
    [UI_MENU_PAD_LAYOUT_AUTO] = "Automatic", [UI_MENU_PAD_LAYOUT_NINTENDO] = "Nintendo (A right)",
    [UI_MENU_PAD_LAYOUT_XBOX] = "Xbox (A bottom)", [UI_MENU_PAD_LAYOUT_XBOX_PROMPTS] = "Xbox prompts (A right)",
    [UI_MENU_PAD_LAYOUT_IN_USE] = "in use: %s",
    [UI_MENU_PAD_LAYOUT_PROMPTS_MISSING] = "prompt files missing: run the setup again",
    [UI_MENU_PAD_LAYOUT_PROMPTS_RESTART] = "prompts change after restart",
    [UI_MENU_STICK_DEADZONE] = "Stick dead zone",
    [UI_MENU_STICK_DEADZONE_DESC] = "How far a gamepad stick moves before the game sees it. Raise it if the character or the camera moves on its own.",
    [UI_MENU_MOUSE_CAMERA] = "Direct mouse camera",
    [UI_MENU_MOUSE_CAMERA_DESC] = "Rotate the normal follow camera directly with mouse movement, without controller "
                                "acceleration or smoothing. Other camera modes use controller-style mouse input. "
                                "Off restores controller-style input everywhere.",
    [UI_MENU_MOUSE_SENS] = "Mouse camera sensitivity",
    [UI_MENU_MOUSE_SENS_DESC] = "How fast the mouse turns the camera.",

    [UI_BINDS_HELP] = "Select a cell and press A or Enter, then the key, mouse button or controller button. Delete or "
                      "X clears a cell. A key in use moves here; a controller button swaps with the one it replaces.",
    [UI_BINDS_COL_KEY1] = "Key 1", [UI_BINDS_COL_KEY2] = "Key 2", [UI_BINDS_COL_PAD] = "Controller",
    [UI_BINDS_GROUP_BUTTONS] = "Buttons", [UI_BINDS_GROUP_MOVE] = "Movement", [UI_BINDS_GROUP_CAMERA] = "Camera",
    [UI_BINDS_PRESS_KEY] = "Press a key or mouse button for %s",
    [UI_BINDS_PRESS_PAD] = "Press a controller button for %s",
    [UI_BINDS_CANCEL_HINT] = "Esc cancels, or wait %d s",
    [UI_BINDS_TAKEN] = "%s was taken from %s", [UI_BINDS_SWAPPED] = "%s swapped with %s",
    [UI_BINDS_RESET] = "Reset all buttons",
    [UI_BINDS_RESET_DESC] = "Back to the default keys and controller buttons. The button layout above applies again: remapped controller buttons stay where you put them until this reset.",
    [UI_BIND_A] = "A", [UI_BIND_B] = "B", [UI_BIND_X] = "X", [UI_BIND_Y] = "Y",
    [UI_BIND_LCLICK] = "Left stick click", [UI_BIND_RCLICK] = "Right stick click", [UI_BIND_L] = "L",
    [UI_BIND_R] = "R", [UI_BIND_ZL] = "ZL", [UI_BIND_ZR] = "ZR", [UI_BIND_PLUS] = "Plus", [UI_BIND_MINUS] = "Minus",
    [UI_BIND_LEFT] = "D-pad left", [UI_BIND_UP] = "D-pad up", [UI_BIND_RIGHT] = "D-pad right",
    [UI_BIND_DOWN] = "D-pad down",
    [UI_BIND_LSTICK_LEFT] = "Move left", [UI_BIND_LSTICK_UP] = "Move forward", [UI_BIND_LSTICK_RIGHT] = "Move right",
    [UI_BIND_LSTICK_DOWN] = "Move back",
    [UI_BIND_RSTICK_LEFT] = "Camera left", [UI_BIND_RSTICK_UP] = "Camera up", [UI_BIND_RSTICK_RIGHT] = "Camera right",
    [UI_BIND_RSTICK_DOWN] = "Camera down",
    [UI_BIND_DESC_A] = "Interact, pick up, talk.", [UI_BIND_DESC_B] = "Run, cancel, go back.",
    [UI_BIND_DESC_X] = "Jump.", [UI_BIND_DESC_Y] = "Attack.", [UI_BIND_DESC_LCLICK] = "Crouch.",
    [UI_BIND_DESC_RCLICK] = "First-person view.", [UI_BIND_DESC_L] = "Abilities.",
    [UI_BIND_DESC_R] = "Throw a weapon.", [UI_BIND_DESC_ZL] = "Shield, lock on.", [UI_BIND_DESC_ZR] = "Bow.",
    [UI_BIND_DESC_PLUS] = "Menu, inventory.", [UI_BIND_DESC_MINUS] = "Map.",
    [UI_BIND_DESC_DPAD] = "Quick-select.",
    [UI_BIND_DESC_MOVE] = "Moves the character like the left stick. The controller's left stick always works too.",
    [UI_BIND_DESC_CAMERA] = "Turns the camera like the right stick. The mouse and the controller's right stick always "
                            "work too.",
    [UI_KEY_SPACE] = "Space", [UI_KEY_TAB] = "Tab", [UI_KEY_LSHIFT] = "Left Shift", [UI_KEY_LCTRL] = "Left Ctrl",
    [UI_KEY_UP] = "Up arrow", [UI_KEY_DOWN] = "Down arrow", [UI_KEY_LEFT] = "Left arrow",
    [UI_KEY_RIGHT] = "Right arrow",
    [UI_KEY_MOUSE_L] = "Left mouse", [UI_KEY_MOUSE_R] = "Right mouse", [UI_KEY_MOUSE_M] = "Middle mouse",
    [UI_KEY_MOUSE_X1] = "Mouse 4", [UI_KEY_MOUSE_X2] = "Mouse 5",
    [UI_KEY_ENTER] = "Enter", [UI_KEY_BACKSPACE] = "Backspace", [UI_KEY_RSHIFT] = "Right Shift",
    [UI_KEY_RCTRL] = "Right Ctrl", [UI_KEY_LALT] = "Alt", [UI_KEY_CAPSLOCK] = "Caps Lock", [UI_KEY_INSERT] = "Insert",
    [UI_KEY_DELETE] = "Delete", [UI_KEY_HOME] = "Home", [UI_KEY_END] = "End", [UI_KEY_PAGEUP] = "Page Up",
    [UI_KEY_PAGEDOWN] = "Page Down", [UI_KEY_NUM_FMT] = "Num %s",
    [UI_PAD_SOUTH] = "Bottom button", [UI_PAD_EAST] = "Right button", [UI_PAD_WEST] = "Left button",
    [UI_PAD_NORTH] = "Top button", [UI_PAD_LSTICK] = "Left stick", [UI_PAD_RSTICK] = "Right stick",
    [UI_PAD_L] = "Left bumper", [UI_PAD_R] = "Right bumper", [UI_PAD_START] = "Start", [UI_PAD_BACK] = "Back",
    [UI_PAD_DPAD_LEFT] = "D-pad left", [UI_PAD_DPAD_UP] = "D-pad up", [UI_PAD_DPAD_RIGHT] = "D-pad right",
    [UI_PAD_DPAD_DOWN] = "D-pad down", [UI_PAD_ZL] = "Left trigger", [UI_PAD_ZR] = "Right trigger",
    [UI_PAD_LPADDLE1] = "Left back button", [UI_PAD_RPADDLE1] = "Right back button",
    [UI_PAD_LPADDLE2] = "Left back button 2", [UI_PAD_RPADDLE2] = "Right back button 2",
    [UI_PAD_CROSS] = "Cross", [UI_PAD_CIRCLE] = "Circle", [UI_PAD_SQUARE] = "Square", [UI_PAD_TRIANGLE] = "Triangle",

    [UI_MENU_MENU_LANG] = "Menu language", [UI_MENU_MENU_LANG_DESC] = "Language of this menu and the overlay.",
    [UI_MENU_MENU_LANG_AUTO] = "Automatic (from Windows)",
    [UI_MENU_THEME] = "Menu theme", [UI_MENU_THEME_DK] = "Donkey Kong", [UI_MENU_THEME_CLASSIC] = "Classic",
    [UI_MENU_THEME_DESC] = "Look of this menu: Donkey Kong (bark and banana yellow) or the classic dark green and gold.",

    [UI_MENU_ADV_WARNING] = "These settings are for testing and troubleshooting. Change them only when you know why.",
    [UI_MENU_GROUP_SCHED] = "Scheduling", [UI_MENU_GROUP_SHADERS] = "Shaders and pipelines",
    [UI_MENU_GROUP_TEXMEM] = "Textures and memory", [UI_MENU_GROUP_FIXES] = "Render fixes",
    [UI_MENU_GROUP_VULKAN] = "Vulkan", [UI_MENU_GROUP_DIAG] = "Diagnostics",
    [UI_MENU_ASYNC_PREEMPT] = "Virtual core preemption",
    [UI_MENU_ASYNC_PREEMPT_DESC] = "Lets a game thread interrupt another one on the same virtual core.",
    [UI_MENU_PREEMPT_OFF] = "Off", [UI_MENU_PREEMPT_ON] = "On", [UI_MENU_PREEMPT_INHERIT] = "Inheritance only",
    [UI_MENU_REPLAY_ASYNC] = "GPU frame prep on its own thread",
    [UI_MENU_REPLAY_ASYNC_DESC] = "Prepares frames for the GPU on a separate thread instead of the game thread. Adds up to one frame of latency.",
    [UI_MENU_TIMER_1MS] = "1 ms Windows timer",
    [UI_MENU_TIMER_1MS_DESC] = "Raises the system timer resolution to 1 ms for steadier frame pacing.",
    [UI_MENU_PIPE_WAIT] = "Waiting draw handling",
    [UI_MENU_PIPE_WAIT_DESC] = "Default keeps the current scheduling with a reserved worker for waiting draws; Old (draw) uses the previous scheduling.",
    [UI_MENU_PIPE_WAIT_DEFAULT] = "Default", [UI_MENU_PIPE_WAIT_DRAW] = "Old (draw)",
    [UI_MENU_PIPE_SKIP] = "Draws skipped while a pipeline builds",
    [UI_MENU_PIPE_SKIP_DESC] = "Which draws are dropped instead of waiting for their pipeline.",
    [UI_MENU_PIPE_SKIP_STRICT] = "Strict", [UI_MENU_PIPE_SKIP_ANY] = "Any",
    [UI_MENU_PIPE_SKIP_FULLSCREEN] = "Fullscreen passes",
    [UI_MENU_LIB_PREBUILD] = "Stage library prebuild",
    [UI_MENU_LIB_PREBUILD_DESC] = "Builds shader stage libraries ahead of time in the background.",
    [UI_MENU_LIB_PREBUILD_THREADS] = "Stage library prebuild threads",
    [UI_MENU_LIB_PREBUILD_THREADS_DESC] = "Threads used for the prebuild; Automatic picks a number from your CPU.",
    [UI_MENU_PIPE_PREBUILD] = "Pipeline prebuild",
    [UI_MENU_PIPE_PREBUILD_DESC] = "Builds known pipelines ahead of time.",
    [UI_MENU_PREWARM_XLATE] = "Translator prewarm",
    [UI_MENU_PREWARM_XLATE_DESC] = "Threads that translate known shaders at start; Automatic takes half the CPU threads, at most 16.",
    [UI_MENU_GPL] = "Graphics pipeline library",
    [UI_MENU_GPL_DESC] = "Uses the Vulkan pipeline library extension; turn off if your driver misbehaves with it.",
    [UI_MENU_ASTC] = "ASTC texture decoding", [UI_MENU_ASTC_DESC] = "Where compressed ASTC textures are decoded.",
    [UI_MENU_ASTC_AUTO] = "Automatic", [UI_MENU_ASTC_GPU] = "GPU shader", [UI_MENU_ASTC_NATIVE] = "Native",
    [UI_MENU_ASTC_CPU] = "CPU",
    [UI_MENU_MEMORY] = "GPU memory mode", [UI_MENU_MEMORY_DESC] = "How game memory is shared with the GPU.",
    [UI_MENU_MEMORY_AUTO] = "Automatic", [UI_MENU_MEMORY_IMPORT] = "Import", [UI_MENU_MEMORY_MIRROR] = "Mirror",
    [UI_MENU_MEMORY_VERIFY] = "Verify",
    [UI_MENU_TEX_ALIAS] = "Texture aliasing", [UI_MENU_TEX_ALIAS_DESC] = "A new texture created over a live texture's memory starts with that content instead of empty.",
    [UI_MENU_TEX_VRAM_PRESSURE] = "Texture memory limit", [UI_MENU_TEX_VRAM_PRESSURE_DESC] = "Percent of video memory above which textures the game released are dropped (they come back from storage when used again). 0 keeps them.",
    [UI_MENU_ASTC_BC] = "Compress ASTC to BC3", [UI_MENU_ASTC_BC_DESC] = "With the GPU decoder: re-encodes decoded ASTC textures to BC3, a quarter of the video memory. For GPUs with 4 GB or less; textures look slightly blockier.",
    [UI_MENU_LOD_GUARD] = "LOD guard", [UI_MENU_LOD_GUARD_DESC] = "Prevents objects flashing in the wrong place or a flat far field for a frame, by waiting for the game to finish rewriting level-of-detail data before the GPU reads it.",
    [UI_MENU_FIX_C0] = "Constant buffer fix",
    [UI_MENU_FIX_C0_DESC] = "Fills the driver-reserved constant bank c0 (storage sizes, dispatch size, point size, base instance) instead of reading zeros.",
    [UI_MENU_FIX_ZERO] = "Zero new GPU memory",
    [UI_MENU_FIX_ZERO_DESC] = "New GPU memory pools start zero-filled, as Switch memory does.",
    [UI_MENU_LIST_RESTART] = "Primitive restart on lists",
    [UI_MENU_LIST_RESTART_DESC] = "Applies the game's primitive restart to triangle-list draws, as the Switch does. Without it the restart indices are drawn as vertices: long stretched triangles, for example on the map. Needs a GPU with primitiveTopologyListRestart.",
    [UI_MENU_BIND_SKIP] = "Skip repeated binds",
    [UI_MENU_BIND_SKIP_DESC] = "Optimisation: skips bindings that did not change.",
    [UI_MENU_SHADER_MEMO] = "Shader memo", [UI_MENU_SHADER_MEMO_DESC] = "Optimisation: remembers shader lookups.",
    [UI_MENU_PIPE_MEMO] = "Pipeline memo", [UI_MENU_PIPE_MEMO_DESC] = "Optimisation: reuses the previous draw's pipeline when its state repeats.",
    [UI_MENU_TEX_MEMO] = "Texture binding memo", [UI_MENU_TEX_MEMO_DESC] = "Optimisation: remembers texture descriptors between draws.",
    [UI_MENU_DESC_TEMPLATE] = "Descriptor templates",
    [UI_MENU_DESC_TEMPLATE_DESC] = "Optimisation: updates descriptors in bulk.",
    [UI_MENU_PUSH_SKIP] = "Skip repeated push constants",
    [UI_MENU_PUSH_SKIP_DESC] = "Optimisation: skips push constants that did not change.",
    [UI_MENU_SPLIT_DRAWS] = "Early GPU submission",
    [UI_MENU_SPLIT_DRAWS_DESC] = "Submits GPU work in parts every N draws between passes so the GPU starts earlier; 0 turns it off.",
    [UI_MENU_VK_VALIDATION] = "Validation layer",
    [UI_MENU_VK_VALIDATION_DESC] = "Checks every Vulkan call and slows the game down a lot.",
    [UI_MENU_VK_VALIDATION_MISSING] = "The validation layer is not installed.",
    [UI_MENU_VK_DYN_VERTEX] = "Dynamic vertex state",
    [UI_MENU_VK_DYN_VERTEX_DESC] = "Forces dynamic vertex input on or off; the default depends on the GPU.",
    [UI_MENU_VK_DYN_A2C] = "Dynamic alpha to coverage",
    [UI_MENU_VK_DYN_A2C_DESC] = "Forces dynamic alpha to coverage on or off. Default: off on AMD's driver for RDNA 2 and "
                                "older, which draws characters black with it, on elsewhere.",
    [UI_MENU_VK_SUBGROUP_PIN] = "Pin subgroup size", [UI_MENU_VK_SUBGROUP_PIN_DESC] = "Fixes the shader subgroup size.",
    [UI_MENU_TRACE] = "Performance log", [UI_MENU_TRACE_DESC] = "Writes frame timings to a file.",
    [UI_MENU_FLIGHT] = "Flight recorder",
    [UI_MENU_FLIGHT_DESC] = "Keeps a short history and saves it when a frame freezes.",
    [UI_MENU_FLIGHT_SPIKE_MS] = "Freeze threshold (ms)",
    [UI_MENU_FLIGHT_SPIKE_MS_DESC] = "Frame time that counts as a freeze.",
    [UI_MENU_INPUT_RECORD] = "Input recording",
    [UI_MENU_INPUT_RECORD_DESC] = "Writes your inputs to a file for bug reports.",
    [UI_MENU_OPEN_LOG] = "Open log folder", [UI_MENU_OPEN_LOG_DESC] = "Shows the folder with the log files.",
    [UI_MENU_COPY_INFO] = "Copy system info",
    [UI_MENU_COPY_INFO_DESC] = "Copies GPU, driver, CPU, RAM and changed settings to the clipboard.",
    [UI_MENU_COPY_INFO_DONE] = "Copied", [UI_MENU_ACTION_FAILED] = "did not work",
    [UI_MENU_RESET] = "Reset all settings to defaults", [UI_MENU_RESET_DESC] = "Applies to every tab.",
    [UI_MENU_RESET_CONFIRM] = "Reset every setting to its default?", [UI_MENU_RESET_YES] = "Reset",
    [UI_MENU_RESET_NO] = "Cancel",

    [UI_MENU_OV_FPS] = "%.0f FPS", [UI_MENU_OV_FRAME_MS] = "%.1f ms", [UI_MENU_OV_GAME_PCT] = "game %.0f%%",
    [UI_MENU_OV_MAX_MS] = "max %.1f ms",
    [UI_MENU_OV_GPU] = "GPU", [UI_MENU_OV_CPU] = "CPU", [UI_MENU_OV_RAM] = "RAM", [UI_MENU_OV_VRAM] = "VRAM",
    [UI_MENU_OV_TEMP] = "Temperature", [UI_MENU_OV_POWER] = "Power", [UI_MENU_OV_CLOCK] = "Clock",
    [UI_MENU_OV_NONE] = "-",
    [UI_MENU_OV_REPLAY] = "GPU frame prep", [UI_MENU_OV_GPU_TIME] = "GPU busy per frame",
    [UI_MENU_OV_PIPE_WAIT] = "Pipeline waits",
    [UI_MENU_OV_SHADER_WAIT] = "Shader waits", [UI_MENU_OV_TEX_UPLOADS] = "Texture uploads",
    [UI_MENU_OV_FMT_PCT] = "%.0f%%", [UI_MENU_OV_FMT_TEMP] = "%.0f °C", [UI_MENU_OV_FMT_POWER] = "%.0f W",
    [UI_MENU_OV_FMT_MHZ] = "%.0f MHz", [UI_MENU_OV_FMT_GB] = "%.1f GB", [UI_MENU_OV_FMT_GB_OF] = "/ %.1f GB",
    [UI_MENU_OV_GAME_GB] = "game %.1f GB", [UI_MENU_OV_FMT_MS] = "%.2f ms", [UI_MENU_OV_PER_FRAME] = "%.1f per frame",
    [UI_MENU_OV_WINDOW] = "Window", [UI_MENU_OV_OUTPUT] = "Game image", [UI_MENU_OV_SCENE] = "3D scene",
    [UI_MENU_OV_SCALING] = "Scaling", [UI_MENU_OV_LIMIT] = "Frame limit", [UI_MENU_OV_PRESENT] = "Presentation",
    [UI_MENU_OV_FMT_SIZE] = "%ux%u", [UI_MENU_OV_FMT_SCENE] = "%ux%u, %.0f%%",
    [UI_MENU_OV_FMT_UPSCALE] = "%ux%u -> %ux%u",
    [UI_MENU_OV_NO_UPSCALER] = "no upscaler",
    [UI_MENU_OV_FMT_FSR] = "AMD FSR 1, sharpness %d%%", [UI_MENU_OV_SCALE_NONE] = "none (1:1)",
    [UI_MENU_OV_SCALE_BILINEAR] = "bilinear",
};

static const char *const text_ru[UI_TEXT_COUNT] = {
    SWKBD("Ввод текста", "Введите имя", "Лошадь", "ОК", "Отмена", "Контроллер: A — ОК, B — отмена"),

    [UI_MENU_TITLE] = "Настройки",
    [UI_MENU_TAB_SCREEN] = "Экран", [UI_MENU_TAB_GAME] = "Игра", [UI_MENU_TAB_SOUND] = "Звук",
    [UI_MENU_TAB_CONTROLS] = "Управление", [UI_MENU_TAB_MENU] = "Меню", [UI_MENU_TAB_ADVANCED] = "Дополнительно",
    [UI_MENU_CONTINUE] = "Продолжить", [UI_MENU_QUIT] = "Выйти из игры",
    [UI_MENU_ON] = "Вкл.", [UI_MENU_OFF] = "Выкл.", [UI_MENU_AUTO] = "Автоматически",
    [UI_MENU_DEFAULT_MARK] = "по умолчанию",
    [UI_MENU_AFTER_RESTART] = "после перезапуска", [UI_MENU_SET_BY_ENV] = "задано переменной окружения",
    [UI_MENU_SAVE_FAILED] = "не удалось сохранить настройки",
    [UI_MENU_VERSION] = "DKB Recomp %s, игра %s",

    [UI_MENU_FULLSCREEN] = "Полный экран", [UI_MENU_FULLSCREEN_DESC] = "Игра на весь экран; F11 переключает режим.",
    [UI_MENU_WINDOW_MODE] = "Режим окна",
    [UI_MENU_WINDOW_MODE_DESC] = "Оконный, без рамки или полный экран; F11 переключает между окном и последним полноэкранным режимом.",
    [UI_MENU_WINDOW_MODE_WINDOWED] = "В окне", [UI_MENU_WINDOW_MODE_BORDERLESS] = "Без рамки",
    [UI_MENU_WINDOW_MODE_FULLSCREEN] = "Полный экран",
    [UI_MENU_WINDOW_SIZE] = "Размер окна", [UI_MENU_WINDOW_SIZE_DESC] = "Размер окна игры вне полноэкранного режима.",
    [UI_MENU_WINDOW_SIZE_CUSTOM] = "Другой",
    [UI_MENU_WINDOWED_ONLY] = "Доступно только в оконном режиме",
    [UI_MENU_PRESENT] = "Вывод кадров", [UI_MENU_PRESENT_DESC] = "Как готовые кадры попадают на экран.",
    [UI_MENU_PRESENT_MAILBOX] = "Mailbox", [UI_MENU_PRESENT_VSYNC] = "Вертикальная синхронизация",
    [UI_MENU_PRESENT_IMMEDIATE] = "Без ожидания",
    [UI_MENU_PRESENT_IN_USE] = "сейчас: %s",
    [UI_MENU_GPU] = "Видеокарта", [UI_MENU_GPU_DESC] = "Устройство Vulkan, которое рисует игру.",
    [UI_MENU_GPU_AUTO] = "Автоматически",
    [UI_MENU_PERF_OVERLAY] = "Индикатор производительности",
    [UI_MENU_PERF_OVERLAY_DESC] = "Частота кадров в правом верхнем углу. «Полный» добавляет время кадров с графиком, "
                                  "загрузку ЦП, GPU и памяти, температуры и ожидания потока отрисовки. F10 переключает "
                                  "режимы.",
    [UI_MENU_PERF_OFF] = "Выкл.", [UI_MENU_PERF_FPS] = "FPS", [UI_MENU_PERF_FULL] = "Полный",
    [UI_PERF_LINE_FPS] = "%.0f к/с",
    [UI_MENU_GRAPHICS] = "Изображение",
    [UI_MENU_OUTPUT_RES] = "Разрешение рендеринга",
    [UI_MENU_ULTRAWIDE] = "Ультраширокий экран",
    [UI_MENU_ULTRAWIDE_DESC] = "Расширяет обзор камеры по горизонтали для 21:9 или 32:9. Меню сохраняют пропорции. "
                              "При выключении используется обзор 16:9. Выбирается разрешение с ближайшей высотой. Требуется перезапуск.",
    [UI_MENU_WIDE_RES] = "Разрешение отрисовки",
    [UI_MENU_WIDE_RES_DESC] = "Разрешение игры до масштабирования под окно или экран. Более высокое улучшает "
                             "детализацию, но требует больше ресурсов GPU и видеопамяти. "
                             "Ультраширокий режим добавляет варианты 21:9 и 32:9. Требуется перезапуск.",
    [UI_MENU_HUD_POSITION] = "Положение интерфейса",
    [UI_MENU_HUD_POSITION_DESC] = "Расположить угловые индикаторы у краёв экрана или в центральной области 16:9. "
                                 "Меню и прицел остаются в центре. Требуется перезапуск.",
    [UI_MENU_HUD_EDGES] = "У краёв экрана", [UI_MENU_HUD_CENTERED] = "По центру (16:9)",
    [UI_MENU_WIDE_DISABLED] = "Сначала включите ультраширокий экран",
    [UI_MENU_OUTPUT_RES_DESC] = "Размер, в котором игра рисует картинку и интерфейс. Выше 1920 × 1080 игре нужно больше "
                                "видеопамяти и времени GPU. Более широкий размер (21:9) расширяет обзор камеры и "
                                "переносит интерфейс в углы экрана, более высокий (16:10, Steam Deck) переносит его к "
                                "верхнему и нижнему краям.",
    [UI_MENU_RENDER_SCALE] = "Масштаб 3D-отрисовки",
    [UI_MENU_RENDER_SCALE_DESC] = "Размер 3D-сцены относительно разрешения игры; FSR игры доводит её до этого размера. "
                                  "Выше 100 % сглаживает лесенки на краях (суперсэмплинг) ценой большой нагрузки на GPU.",
    [UI_MENU_RENDER_SCALE_GAME] = "Как в игре (83 %)",
    [UI_MENU_RAIDERS_RES] = "Разрешение",
    [UI_MENU_RAIDERS_RES_DESC] = "Вывод, под который рисует игра, как Switch 2 на телевизоре с таким разрешением: в "
                                 "док-станции 1920 × 1080, 2560 × 1440 или 3840 × 2160; в портативном режиме 1280 × 720 "
                                 "при 1080p и 1920 × 1080 выше. Чем выше, тем больше видеопамяти и времени GPU.",
    [UI_MENU_UPSCALER] = "Масштабирование под окно",
    [UI_MENU_UPSCALER_DESC] = "Как картинка растягивается, когда окно больше разрешения игры. FSR сохраняет края и "
                              "детали чётче билинейного.",
    [UI_MENU_UPSCALER_FSR] = "AMD FSR 1", [UI_MENU_UPSCALER_BILINEAR] = "Билинейное",
    [UI_MENU_ASPECT] = "Подгонка картинки под окно",
    [UI_MENU_ASPECT_DESC] = "Когда форма окна отличается от разрешения игры (картинка 16:9 на экране 21:9): сохранить "
                            "форму картинки с чёрными полосами или растянуть её на всё окно. Обзор камеры не расширяется.",
    [UI_MENU_ASPECT_FIT] = "Чёрные полосы", [UI_MENU_ASPECT_STRETCH] = "Растянуть",
    [UI_MENU_FOV] = "Поле зрения",
    [UI_MENU_FOV_DESC] = "Сколько мира показывает камера, в процентах от игрового. Только для версии игры 1.4.3.",
    [UI_MENU_SHARPNESS] = "Резкость FSR",
    [UI_MENU_SHARPNESS_DESC] = "Повышение резкости после масштабирования FSR; 0 выключает его.",
    [UI_MENU_SMAA] = "Сглаживание (SMAA)",
    [UI_MENU_SMAA_DESC] = "Сглаживает лесенки на готовой картинке после масштабирования под окно и до отрисовки этого "
                          "меню. Интерфейс игры тоже сглаживается. Чем выше качество, тем больше нагрузка на GPU.",
    [UI_MENU_SMAA_LOW] = "Низкое", [UI_MENU_SMAA_MEDIUM] = "Среднее", [UI_MENU_SMAA_HIGH] = "Высокое",
    [UI_MENU_COLOR] = "Цвет",
    [UI_MENU_COLOR_PRESET] = "Цветовой профиль",
    [UI_MENU_COLOR_PRESET_DESC] = "Задаёт четыре значения ниже. «Яркий»: больше контраста и насыщенности, тени немного "
                                  "светлее (для OLED-экранов). «Чёткость»: немного больше контраста, насыщенности и "
                                  "яркости. Изменение любого значения делает профиль «Своим». Интерфейс игры меняется "
                                  "вместе с картинкой, это меню — нет.",
    [UI_MENU_COLOR_DEFAULT] = "Как в игре", [UI_MENU_COLOR_VIVID] = "Яркий (OLED)", [UI_MENU_COLOR_CLARITY] = "Чёткость",
    [UI_MENU_COLOR_CUSTOM] = "Свой",
    [UI_MENU_COLOR_BRIGHTNESS] = "Яркость", [UI_MENU_COLOR_BRIGHTNESS_DESC] = "Прибавляется к каждому цвету; 0 — без изменений.",
    [UI_MENU_COLOR_CONTRAST] = "Контраст",
    [UI_MENU_COLOR_CONTRAST_DESC] = "Отдаляет цвета от среднего серого (выше 100 %) или приближает к нему; 100 % — без "
                                    "изменений.",
    [UI_MENU_COLOR_SATURATION] = "Насыщенность",
    [UI_MENU_COLOR_SATURATION_DESC] = "Сила цвета относительно серого; 100 % — без изменений.",
    [UI_MENU_COLOR_GAMMA] = "Гамма",
    [UI_MENU_COLOR_GAMMA_DESC] = "Ниже 1 осветляет тёмные и средние тона, выше 1 затемняет их; 1 — без изменений.",
    [UI_MENU_HDR_GROUP] = "HDR (экспериментально)",
    [UI_MENU_HDR] = "Вывод HDR",
    [UI_MENU_HDR_DESC] = "Экспериментально: SDR-картинка игры с поднятыми бликами на HDR-дисплее. Нужен включённый HDR в Windows («Использовать HDR») или "
                         "Wayland-композитор с управлением цветом; без него картинка остаётся SDR. Применяется после "
                         "перезапуска.",
    [UI_MENU_HDR_NATIVE] = "Собственный HDR игры",
    [UI_MENU_HDR_NATIVE_DESC] = "При включённом выводе HDR: игра сама строит HDR-картинку, как на консоли, с бликами до "
                                "пиковой яркости (игра останавливается на 1400 нит), вместо поднятой SDR-картинки. FSR, "
                                "SMAA и цветокоррекция в этом режиме выключены. Применяется после перезапуска.",
    [UI_MENU_HDR_PEAK] = "Пиковая яркость",
    [UI_MENU_HDR_PEAK_DESC] = "Пик вашего дисплея в нитах; самые яркие блики останавливаются на нём.",
    [UI_MENU_HDR_GAME] = "Яркость игры",
    [UI_MENU_HDR_GAME_DESC] = "Ниты белого цвета картинки (paper white). 203 — эталон HDR; в светлой комнате "
                              "поднимите.",
    [UI_MENU_HDR_UI] = "Яркость меню",
    [UI_MENU_HDR_UI_DESC] = "Ниты белого цвета этого меню. Интерфейс игры — часть картинки и следует яркости игры.",

    [UI_MENU_FPS_LIMIT] = "Ограничение частоты кадров",
    [UI_MENU_DKB_FPS_LIMIT_DESC] = "При включённой настройке «Скорость игры по часам» изменения применяются сразу. "
                                 "Для частоты выше 60 нужны этот экспериментальный режим и достаточная производительность CPU и GPU. «Без ограничения» — экспериментально.",
    [UI_MENU_FPS_LIMIT_DESC] = "Наибольшее число кадров в секунду. Выше 60 — экспериментальный режим, нужна «Скорость "
                               "игры по часам». «Без ограничения» — экспериментально.",
    [UI_MENU_FPS_30] = "30 FPS", [UI_MENU_FPS_40] = "40 FPS", [UI_MENU_FPS_45] = "45 FPS",
    [UI_MENU_FPS_60] = "60 FPS", [UI_MENU_FPS_90] = "90 FPS", [UI_MENU_FPS_120] = "120 FPS", [UI_MENU_FPS_144] = "144 FPS",
    [UI_MENU_FPS_DISPLAY] = "Как у монитора", [UI_MENU_FPS_DISPLAY_HZ] = "Как у монитора (%.0f Гц)",
    [UI_MENU_FPS_UNLIMITED] = "Без ограничения",
    [UI_MENU_FPS_NEEDS_REALTIME] = "Нужна «Скорость игры по часам» (перезапуск): частоты ниже и выше 60 FPS",
    [UI_MENU_REALTIME] = "Скорость игры по часам",
    [UI_MENU_REALTIME_DESC] = "Игра идёт по реальному времени, медленный кадр её не замедляет.",
    [UI_MENU_DKB_REALTIME_DESC] = "Экспериментальный режим, по умолчанию выключен. Шаг игрового времени зависит от длительности кадра. "
                                "Камера, физика, кат-сцены и звук ещё требуют проверки. Нужен перезапуск.",
    [UI_MENU_CLOTH_REALTIME] = "Ткань по часам",
    [UI_MENU_CLOTH_REALTIME_DESC] = "Ткань и волосы тоже идут по реальному времени.",
    [UI_MENU_HIGHFPS_FIX] = "Исправления для высокого FPS",
    [UI_MENU_HIGHFPS_FIX_DESC] = "Исправляет физику, которая ломается при частоте выше 30 кадров в секунду, и объекты, которые выше 60 остаются полупрозрачными или невидимыми.",
    [UI_MENU_GAME_LANG] = "Язык игры", [UI_MENU_GAME_LANG_DESC] = "Язык текста и озвучки игры.",
    [UI_MENU_GAME_LANG_AUTO] = "Автоматически",
    [UI_MENU_OPERATION_MODE] = "Режим консоли",
    [UI_MENU_OPERATION_MODE_DESC] = "Игра считает, что консоль в док-станции или в портативном режиме.",
    [UI_MENU_MODE_DOCKED] = "В док-станции", [UI_MENU_MODE_HANDHELD] = "Портативный",
    [UI_MENU_ASSUME_FOCUS] = "Работать без фокуса",
    [UI_MENU_ASSUME_FOCUS_DESC] = "Включено (по умолчанию): игра идёт, пока вы в другом окне, а клавиатура не "
                                  "действует, пока окно не вернётся на передний план. Выключено: игра встаёт на паузу, "
                                  "когда окно теряет фокус, как на Switch при нажатии Home; прерванная так загрузка "
                                  "может остановиться насовсем.",

    [UI_MENU_VOLUME] = "Громкость", [UI_MENU_VOLUME_DESC] = "Общая громкость игры.",
    [UI_MENU_AUDIO_CHANNELS] = "Динамики",
    [UI_MENU_AUDIO_CHANNELS_DESC] = "Стерео или объёмный звук 5.1. Автоматически выбирается 5.1, если у устройства "
                                    "вывода по умолчанию шесть каналов или больше. Применяется после перезапуска.",
    [UI_MENU_CHANNELS_AUTO] = "Автоматически", [UI_MENU_CHANNELS_STEREO] = "Стерео", [UI_MENU_CHANNELS_51] = "Объёмный 5.1",

    [UI_MENU_MOTION] = "Управление движением", [UI_MENU_MOTION_DESC] = "Прицеливание гироскопом контроллера.",
    [UI_MENU_RUMBLE_STRENGTH] = "Сила вибрации",
    [UI_MENU_RUMBLE_STRENGTH_DESC] = "Насколько сильно вибрирует контроллер.",
    [UI_MENU_RUMBLE_CURVE] = "Кривая вибрации",
    [UI_MENU_RUMBLE_CURVE_DESC] = "Чем ниже значение, тем сильнее слабая вибрация.",
    [UI_MENU_RUMBLE_TRIGGERS] = "Вибрация триггеров (Xbox)",
    [UI_MENU_RUMBLE_TRIGGERS_DESC] = "Вибрация и в триггерах контроллера Xbox.",
    [UI_MENU_PAD_LAYOUT] = "Раскладка кнопок",
    [UI_MENU_PAD_LAYOUT_DESC] = "Какая кнопка работает как A. Nintendo: правая, как на Switch. Xbox: нижняя, как "
                                "подписано на контроллере Xbox (крест на контроллере PlayStation). Автоматически: "
                                "Nintendo для контроллеров с подписями Nintendo, Xbox для остальных. Подсказки Xbox: "
                                "A справа, как в Nintendo, а игра показывает надписи Xbox (B вместо A, A вместо B, Y "
                                "вместо X, X вместо Y); подсказки меняются после перезапуска. Буквы записаны в файлах "
                                 "игры, поэтому у кнопки, переназначенной на вкладке управления, на экране остаётся "
                                 "прежняя буква.",
    [UI_MENU_PAD_LAYOUT_AUTO] = "Автоматически", [UI_MENU_PAD_LAYOUT_NINTENDO] = "Nintendo (A справа)",
    [UI_MENU_PAD_LAYOUT_XBOX] = "Xbox (A снизу)", [UI_MENU_PAD_LAYOUT_XBOX_PROMPTS] = "Подсказки Xbox (A справа)",
    [UI_MENU_PAD_LAYOUT_IN_USE] = "сейчас: %s",
    [UI_MENU_PAD_LAYOUT_PROMPTS_MISSING] = "нет файлов подсказок: запустите установку снова",
    [UI_MENU_PAD_LAYOUT_PROMPTS_RESTART] = "подсказки сменятся после перезапуска",
    [UI_MENU_STICK_DEADZONE] = "Мёртвая зона стиков",
    [UI_MENU_STICK_DEADZONE_DESC] = "Насколько нужно отклонить стик геймпада, чтобы игра это заметила. Увеличьте, если персонаж или камера двигаются сами.",
    [UI_MENU_MOUSE_SENS] = "Чувствительность мыши",
    [UI_MENU_MOUSE_SENS_DESC] = "Как быстро мышь поворачивает камеру.",

    [UI_BINDS_HELP] = "Выберите ячейку и нажмите A или Enter, затем клавишу, кнопку мыши или кнопку контроллера. "
                      "Delete или X очищает ячейку. Занятая клавиша переходит сюда; кнопка контроллера меняется "
                      "местами с той, которую заменяет.",
    [UI_BINDS_COL_KEY1] = "Клавиша 1", [UI_BINDS_COL_KEY2] = "Клавиша 2", [UI_BINDS_COL_PAD] = "Контроллер",
    [UI_BINDS_GROUP_BUTTONS] = "Кнопки", [UI_BINDS_GROUP_MOVE] = "Движение", [UI_BINDS_GROUP_CAMERA] = "Камера",
    [UI_BINDS_PRESS_KEY] = "Нажмите клавишу или кнопку мыши для «%s»",
    [UI_BINDS_PRESS_PAD] = "Нажмите кнопку контроллера для «%s»",
    [UI_BINDS_CANCEL_HINT] = "Esc отменяет, или подождите %d с",
    [UI_BINDS_TAKEN] = "%s: убрано с «%s»", [UI_BINDS_SWAPPED] = "«%s» и «%s» поменялись местами",
    [UI_BINDS_RESET] = "Сбросить все кнопки",
    [UI_BINDS_RESET_DESC] = "Вернуть клавиши и кнопки контроллера по умолчанию. Снова действует раскладка кнопок выше: "
                            "переназначенные кнопки контроллера остаются на своих местах до "
                            "этого сброса.",
    [UI_BIND_A] = "A", [UI_BIND_B] = "B", [UI_BIND_X] = "X", [UI_BIND_Y] = "Y",
    [UI_BIND_LCLICK] = "Нажатие левого стика", [UI_BIND_RCLICK] = "Нажатие правого стика", [UI_BIND_L] = "L",
    [UI_BIND_R] = "R", [UI_BIND_ZL] = "ZL", [UI_BIND_ZR] = "ZR", [UI_BIND_PLUS] = "Плюс", [UI_BIND_MINUS] = "Минус",
    [UI_BIND_LEFT] = "Крестовина влево", [UI_BIND_UP] = "Крестовина вверх", [UI_BIND_RIGHT] = "Крестовина вправо",
    [UI_BIND_DOWN] = "Крестовина вниз",
    [UI_BIND_LSTICK_LEFT] = "Влево", [UI_BIND_LSTICK_UP] = "Вперёд", [UI_BIND_LSTICK_RIGHT] = "Вправо",
    [UI_BIND_LSTICK_DOWN] = "Назад",
    [UI_BIND_RSTICK_LEFT] = "Камера влево", [UI_BIND_RSTICK_UP] = "Камера вверх",
    [UI_BIND_RSTICK_RIGHT] = "Камера вправо", [UI_BIND_RSTICK_DOWN] = "Камера вниз",
    [UI_BIND_DESC_A] = "Взаимодействие, подобрать, поговорить.", [UI_BIND_DESC_B] = "Бег, отмена, назад.",
    [UI_BIND_DESC_X] = "Прыжок.", [UI_BIND_DESC_Y] = "Атака.", [UI_BIND_DESC_LCLICK] = "Присесть.",
    [UI_BIND_DESC_RCLICK] = "Вид от первого лица.", [UI_BIND_DESC_L] = "Способности.",
    [UI_BIND_DESC_R] = "Бросок оружия.", [UI_BIND_DESC_ZL] = "Щит, захват цели.", [UI_BIND_DESC_ZR] = "Лук.",
    [UI_BIND_DESC_PLUS] = "Меню, инвентарь.", [UI_BIND_DESC_MINUS] = "Карта.",
    [UI_BIND_DESC_DPAD] = "Быстрый выбор.",
    [UI_BIND_DESC_MOVE] = "Двигает персонажа, как левый стик. Левый стик контроллера тоже всегда работает.",
    [UI_BIND_DESC_CAMERA] = "Поворачивает камеру, как правый стик. Мышь и правый стик контроллера тоже всегда "
                            "работают.",
    [UI_KEY_SPACE] = "Пробел", [UI_KEY_TAB] = "Tab", [UI_KEY_LSHIFT] = "Левый Shift", [UI_KEY_LCTRL] = "Левый Ctrl",
    [UI_KEY_UP] = "Стрелка вверх", [UI_KEY_DOWN] = "Стрелка вниз", [UI_KEY_LEFT] = "Стрелка влево",
    [UI_KEY_RIGHT] = "Стрелка вправо",
    [UI_KEY_MOUSE_L] = "ЛКМ", [UI_KEY_MOUSE_R] = "ПКМ", [UI_KEY_MOUSE_M] = "СКМ", [UI_KEY_MOUSE_X1] = "Мышь 4",
    [UI_KEY_MOUSE_X2] = "Мышь 5",
    [UI_KEY_ENTER] = "Enter", [UI_KEY_BACKSPACE] = "Backspace", [UI_KEY_RSHIFT] = "Правый Shift",
    [UI_KEY_RCTRL] = "Правый Ctrl", [UI_KEY_LALT] = "Alt", [UI_KEY_CAPSLOCK] = "Caps Lock", [UI_KEY_INSERT] = "Insert",
    [UI_KEY_DELETE] = "Delete", [UI_KEY_HOME] = "Home", [UI_KEY_END] = "End", [UI_KEY_PAGEUP] = "Page Up",
    [UI_KEY_PAGEDOWN] = "Page Down", [UI_KEY_NUM_FMT] = "Num %s",
    [UI_PAD_SOUTH] = "Нижняя кнопка", [UI_PAD_EAST] = "Правая кнопка", [UI_PAD_WEST] = "Левая кнопка",
    [UI_PAD_NORTH] = "Верхняя кнопка", [UI_PAD_LSTICK] = "Левый стик", [UI_PAD_RSTICK] = "Правый стик",
    [UI_PAD_L] = "Левый бампер", [UI_PAD_R] = "Правый бампер", [UI_PAD_START] = "Start", [UI_PAD_BACK] = "Back",
    [UI_PAD_DPAD_LEFT] = "Крестовина влево", [UI_PAD_DPAD_UP] = "Крестовина вверх",
    [UI_PAD_DPAD_RIGHT] = "Крестовина вправо", [UI_PAD_DPAD_DOWN] = "Крестовина вниз", [UI_PAD_ZL] = "Левый курок",
    [UI_PAD_ZR] = "Правый курок",
    [UI_PAD_LPADDLE1] = "Левая задняя кнопка", [UI_PAD_RPADDLE1] = "Правая задняя кнопка",
    [UI_PAD_LPADDLE2] = "Левая задняя кнопка 2", [UI_PAD_RPADDLE2] = "Правая задняя кнопка 2",
    [UI_PAD_CROSS] = "Крест", [UI_PAD_CIRCLE] = "Круг", [UI_PAD_SQUARE] = "Квадрат", [UI_PAD_TRIANGLE] = "Треугольник",

    [UI_MENU_MENU_LANG] = "Язык меню", [UI_MENU_MENU_LANG_DESC] = "Язык этого меню и индикатора.",
    [UI_MENU_MENU_LANG_AUTO] = "Автоматически (из Windows)",
    [UI_MENU_THEME] = "Тема меню", [UI_MENU_THEME_DK] = "Donkey Kong", [UI_MENU_THEME_CLASSIC] = "Классическая",
    [UI_MENU_THEME_DESC] = "Вид этого меню: Donkey Kong (кора и банановый жёлтый) или классический тёмно-зелёный с золотом.",

    [UI_MENU_ADV_WARNING] = "Эти настройки нужны для тестов и поиска неполадок. Меняйте их, только если знаете зачем.",
    [UI_MENU_GROUP_SCHED] = "Планирование", [UI_MENU_GROUP_SHADERS] = "Шейдеры и конвейеры",
    [UI_MENU_GROUP_TEXMEM] = "Текстуры и память", [UI_MENU_GROUP_FIXES] = "Исправления графики",
    [UI_MENU_GROUP_VULKAN] = "Vulkan", [UI_MENU_GROUP_DIAG] = "Диагностика",
    [UI_MENU_ASYNC_PREEMPT] = "Вытеснение на виртуальных ядрах",
    [UI_MENU_ASYNC_PREEMPT_DESC] = "Позволяет потоку игры прервать другой поток на том же виртуальном ядре.",
    [UI_MENU_PREEMPT_OFF] = "Выкл.", [UI_MENU_PREEMPT_ON] = "Вкл.", [UI_MENU_PREEMPT_INHERIT] = "Только наследование",
    [UI_MENU_REPLAY_ASYNC] = "Подготовка кадра для GPU в отдельном потоке",
    [UI_MENU_REPLAY_ASYNC_DESC] = "Кадр для GPU готовится в отдельном потоке, а не в потоке игры. Добавляет до одного кадра задержки.",
    [UI_MENU_TIMER_1MS] = "Таймер Windows 1 мс",
    [UI_MENU_TIMER_1MS_DESC] = "Повышает точность системного таймера до 1 мс для ровного темпа кадров.",
    [UI_MENU_PIPE_WAIT] = "Обработка ожидающих отрисовок",
    [UI_MENU_PIPE_WAIT_DESC] = "«По умолчанию» — текущее планирование с отдельным потоком для ожидающих отрисовок; «Старый способ» — прежнее планирование.",
    [UI_MENU_PIPE_WAIT_DEFAULT] = "По умолчанию", [UI_MENU_PIPE_WAIT_DRAW] = "Старый способ",
    [UI_MENU_PIPE_SKIP] = "Пропуск отрисовок при сборке конвейера",
    [UI_MENU_PIPE_SKIP_DESC] = "Какие отрисовки отбрасываются, а не ждут свой конвейер.",
    [UI_MENU_PIPE_SKIP_STRICT] = "Строго", [UI_MENU_PIPE_SKIP_ANY] = "Любые",
    [UI_MENU_PIPE_SKIP_FULLSCREEN] = "Полноэкранные проходы",
    [UI_MENU_LIB_PREBUILD] = "Предсборка библиотек стадий",
    [UI_MENU_LIB_PREBUILD_DESC] = "Заранее собирает библиотеки стадий шейдеров в фоне.",
    [UI_MENU_LIB_PREBUILD_THREADS] = "Потоки предсборки библиотек",
    [UI_MENU_LIB_PREBUILD_THREADS_DESC] = "Сколько потоков занято предсборкой; «Автоматически» — по числу ядер процессора.",
    [UI_MENU_PIPE_PREBUILD] = "Предсборка конвейеров",
    [UI_MENU_PIPE_PREBUILD_DESC] = "Заранее собирает известные конвейеры.",
    [UI_MENU_PREWARM_XLATE] = "Прогрев транслятора",
    [UI_MENU_PREWARM_XLATE_DESC] = "Потоки, которые при запуске переводят известные шейдеры; «Автоматически» — половина потоков процессора, не больше 16.",
    [UI_MENU_GPL] = "Библиотека графических конвейеров",
    [UI_MENU_GPL_DESC] = "Использует расширение Vulkan для библиотек конвейеров; выключите, если драйвер с ним сбоит.",
    [UI_MENU_ASTC] = "Распаковка текстур ASTC", [UI_MENU_ASTC_DESC] = "Где распаковываются сжатые текстуры ASTC.",
    [UI_MENU_ASTC_AUTO] = "Автоматически", [UI_MENU_ASTC_GPU] = "Шейдер GPU", [UI_MENU_ASTC_NATIVE] = "Аппаратно",
    [UI_MENU_ASTC_CPU] = "Процессор",
    [UI_MENU_MEMORY] = "Режим памяти GPU", [UI_MENU_MEMORY_DESC] = "Как память игры передаётся видеокарте.",
    [UI_MENU_MEMORY_AUTO] = "Автоматически", [UI_MENU_MEMORY_IMPORT] = "Импорт", [UI_MENU_MEMORY_MIRROR] = "Зеркало",
    [UI_MENU_MEMORY_VERIFY] = "Проверка",
    [UI_MENU_TEX_ALIAS] = "Наследование содержимого текстур", [UI_MENU_TEX_ALIAS_DESC] = "Новая текстура поверх памяти живой текстуры получает её содержимое, а не пустую.",
    [UI_MENU_TEX_VRAM_PRESSURE] = "Предел памяти текстур", [UI_MENU_TEX_VRAM_PRESSURE_DESC] = "Процент видеопамяти, выше которого освобождённые игрой текстуры удаляются (при повторном использовании загружаются снова). 0 — не удалять.",
    [UI_MENU_ASTC_BC] = "Сжатие ASTC в BC3", [UI_MENU_ASTC_BC_DESC] = "С декодером на GPU: декодированные текстуры ASTC пересжимаются в BC3 и занимают вчетверо меньше видеопамяти. Для видеокарт с 4 ГБ и меньше; текстуры чуть более блочные.",
    [UI_MENU_LOD_GUARD] = "Защита LOD", [UI_MENU_LOD_GUARD_DESC] = "Не даёт объектам мелькать не на своём месте, а дальнему плану — на кадр становиться плоским: ждёт, пока игра допишет данные уровней детализации, и только потом их читает GPU.",
    [UI_MENU_FIX_C0] = "Исправление константного буфера",
    [UI_MENU_FIX_C0_DESC] = "Заполняет зарезервированный драйвером банк констант c0 (размеры хранилищ, размер запуска, размер точки, базовый экземпляр) вместо чтения нулей.",
    [UI_MENU_FIX_ZERO] = "Обнулять новую память GPU",
    [UI_MENU_FIX_ZERO_DESC] = "Новые пулы памяти GPU заполняются нулями, как память Switch.",
    [UI_MENU_LIST_RESTART] = "Перезапуск примитивов в списках",
    [UI_MENU_LIST_RESTART_DESC] = "Применяет перезапуск примитивов игры и к спискам треугольников, как на Switch. Без него индексы перезапуска рисуются как вершины: длинные вытянутые треугольники, например на карте. Нужна видеокарта с primitiveTopologyListRestart.",
    [UI_MENU_BIND_SKIP] = "Пропуск повторных привязок",
    [UI_MENU_BIND_SKIP_DESC] = "Оптимизация: пропускает неизменившиеся привязки.",
    [UI_MENU_SHADER_MEMO] = "Память шейдеров", [UI_MENU_SHADER_MEMO_DESC] = "Оптимизация: запоминает поиск шейдеров.",
    [UI_MENU_PIPE_MEMO] = "Память конвейера", [UI_MENU_PIPE_MEMO_DESC] = "Оптимизация: повторяет конвейер прошлой отрисовки, если её состояние не изменилось.",
    [UI_MENU_TEX_MEMO] = "Память привязок текстур", [UI_MENU_TEX_MEMO_DESC] = "Оптимизация: запоминает дескрипторы текстур между отрисовками.",
    [UI_MENU_DESC_TEMPLATE] = "Шаблоны дескрипторов",
    [UI_MENU_DESC_TEMPLATE_DESC] = "Оптимизация: обновляет дескрипторы пачками.",
    [UI_MENU_PUSH_SKIP] = "Пропуск повторных push-констант",
    [UI_MENU_PUSH_SKIP_DESC] = "Оптимизация: пропускает неизменившиеся push-константы.",
    [UI_MENU_SPLIT_DRAWS] = "Ранняя отправка работы GPU",
    [UI_MENU_SPLIT_DRAWS_DESC] = "Отправляет работу GPU частями через каждые N отрисовок между проходами, чтобы GPU начинал раньше; 0 — выключено.",
    [UI_MENU_VK_VALIDATION] = "Слой проверки",
    [UI_MENU_VK_VALIDATION_DESC] = "Проверяет каждый вызов Vulkan и сильно замедляет игру.",
    [UI_MENU_VK_VALIDATION_MISSING] = "Слой проверки не установлен.",
    [UI_MENU_VK_DYN_VERTEX] = "Динамическое состояние вершин",
    [UI_MENU_VK_DYN_VERTEX_DESC] = "Принудительно включает или выключает динамический ввод вершин; по умолчанию зависит от GPU.",
    [UI_MENU_VK_DYN_A2C] = "Динамический alpha to coverage",
    [UI_MENU_VK_DYN_A2C_DESC] = "Принудительно включает или выключает динамический alpha to coverage. По умолчанию "
                                "выключен на драйвере AMD для RDNA 2 и старше, где с ним персонажи чёрные, и включён "
                                "на остальных.",
    [UI_MENU_VK_SUBGROUP_PIN] = "Фиксировать размер подгруппы",
    [UI_MENU_VK_SUBGROUP_PIN_DESC] = "Фиксирует размер подгруппы шейдера.",
    [UI_MENU_TRACE] = "Журнал производительности", [UI_MENU_TRACE_DESC] = "Пишет время кадров в файл.",
    [UI_MENU_FLIGHT] = "Запись последних кадров (для отчёта о фризе)",
    [UI_MENU_FLIGHT_DESC] = "Хранит короткую историю и сохраняет её, когда кадр подвисает.",
    [UI_MENU_FLIGHT_SPIKE_MS] = "Порог подвисания (мс)",
    [UI_MENU_FLIGHT_SPIKE_MS_DESC] = "Время кадра, которое считается подвисанием.",
    [UI_MENU_INPUT_RECORD] = "Запись ввода",
    [UI_MENU_INPUT_RECORD_DESC] = "Пишет ваш ввод в файл для отчёта об ошибке.",
    [UI_MENU_OPEN_LOG] = "Открыть папку журналов", [UI_MENU_OPEN_LOG_DESC] = "Показывает папку с файлами журнала.",
    [UI_MENU_COPY_INFO] = "Копировать сведения о системе",
    [UI_MENU_COPY_INFO_DESC] = "Копирует в буфер обмена видеокарту, драйвер, процессор, память и изменённые настройки.",
    [UI_MENU_COPY_INFO_DONE] = "Скопировано", [UI_MENU_ACTION_FAILED] = "не получилось",
    [UI_MENU_RESET] = "Сбросить все настройки", [UI_MENU_RESET_DESC] = "Действует на все вкладки.",
    [UI_MENU_RESET_CONFIRM] = "Сбросить все настройки к значениям по умолчанию?", [UI_MENU_RESET_YES] = "Сбросить",
    [UI_MENU_RESET_NO] = "Отмена",

    [UI_MENU_OV_FPS] = "%.0f FPS", [UI_MENU_OV_FRAME_MS] = "%.1f мс", [UI_MENU_OV_GAME_PCT] = "игра %.0f%%",
    [UI_MENU_OV_MAX_MS] = "макс. %.1f мс",
    [UI_MENU_OV_GPU] = "GPU", [UI_MENU_OV_CPU] = "ЦП", [UI_MENU_OV_RAM] = "ОЗУ", [UI_MENU_OV_VRAM] = "Видеопамять",
    [UI_MENU_OV_TEMP] = "Температура", [UI_MENU_OV_POWER] = "Мощность", [UI_MENU_OV_CLOCK] = "Частота",
    [UI_MENU_OV_NONE] = "-",
    [UI_MENU_OV_REPLAY] = "Подготовка кадра для GPU", [UI_MENU_OV_GPU_TIME] = "Занятость GPU за кадр",
    [UI_MENU_OV_PIPE_WAIT] = "Ожидания конвейеров",
    [UI_MENU_OV_SHADER_WAIT] = "Ожидания шейдеров", [UI_MENU_OV_TEX_UPLOADS] = "Загрузки текстур",
    [UI_MENU_OV_FMT_PCT] = "%.0f%%", [UI_MENU_OV_FMT_TEMP] = "%.0f °C", [UI_MENU_OV_FMT_POWER] = "%.0f Вт",
    [UI_MENU_OV_FMT_MHZ] = "%.0f МГц", [UI_MENU_OV_FMT_GB] = "%.1f ГБ", [UI_MENU_OV_FMT_GB_OF] = "/ %.1f ГБ",
    [UI_MENU_OV_GAME_GB] = "игра %.1f ГБ", [UI_MENU_OV_FMT_MS] = "%.2f мс", [UI_MENU_OV_PER_FRAME] = "%.1f за кадр",
    [UI_MENU_OV_WINDOW] = "Окно", [UI_MENU_OV_OUTPUT] = "Изображение игры", [UI_MENU_OV_SCENE] = "3D-сцена",
    [UI_MENU_OV_SCALING] = "Масштабирование", [UI_MENU_OV_LIMIT] = "Лимит кадров", [UI_MENU_OV_PRESENT] = "Вывод кадров",
    [UI_MENU_OV_FMT_SIZE] = "%ux%u", [UI_MENU_OV_FMT_SCENE] = "%ux%u, %.0f%%",
    [UI_MENU_OV_FMT_UPSCALE] = "%ux%u -> %ux%u",
    [UI_MENU_OV_NO_UPSCALER] = "без апскейлера",
    [UI_MENU_OV_FMT_FSR] = "AMD FSR 1, резкость %d%%", [UI_MENU_OV_SCALE_NONE] = "нет (1:1)",
    [UI_MENU_OV_SCALE_BILINEAR] = "билинейное",
};

static const char *const text_ko[UI_TEXT_COUNT] = {
    SWKBD("문자 입력", "이름을 입력하세요", "말", "확인", "취소", "컨트롤러: A 확인, B 취소"),

    [UI_MENU_TITLE] = "설정",
    [UI_MENU_TAB_SCREEN] = "화면", [UI_MENU_TAB_GAME] = "게임", [UI_MENU_TAB_SOUND] = "사운드",
    [UI_MENU_TAB_CONTROLS] = "조작", [UI_MENU_TAB_MENU] = "메뉴", [UI_MENU_TAB_ADVANCED] = "고급",
    [UI_MENU_CONTINUE] = "계속하기", [UI_MENU_QUIT] = "게임 종료",
    [UI_MENU_ON] = "켜기", [UI_MENU_OFF] = "끄기", [UI_MENU_AUTO] = "자동", [UI_MENU_DEFAULT_MARK] = "기본값",
    [UI_MENU_AFTER_RESTART] = "재시작 후 적용", [UI_MENU_SET_BY_ENV] = "환경 변수로 설정됨",
    [UI_MENU_SAVE_FAILED] = "설정을 저장하지 못했습니다",
    [UI_MENU_VERSION] = "DKB Recomp %s, 게임 %s",

    [UI_MENU_FULLSCREEN] = "전체 화면", [UI_MENU_FULLSCREEN_DESC] = "게임을 화면 가득 표시합니다. F11로 전환합니다.",
    [UI_MENU_WINDOW_MODE] = "창 모드",
    [UI_MENU_WINDOW_MODE_DESC] = "창 모드, 테두리 없는 창, 또는 전체 화면입니다. F11은 창 모드와 마지막 전체 화면 모드 사이를 전환합니다.",
    [UI_MENU_WINDOW_MODE_WINDOWED] = "창 모드", [UI_MENU_WINDOW_MODE_BORDERLESS] = "테두리 없음",
    [UI_MENU_WINDOW_MODE_FULLSCREEN] = "전체 화면",
    [UI_MENU_WINDOW_SIZE] = "창 크기", [UI_MENU_WINDOW_SIZE_DESC] = "전체 화면이 아닐 때 게임 창의 크기입니다.",
    [UI_MENU_WINDOW_SIZE_CUSTOM] = "사용자 지정",
    [UI_MENU_WINDOWED_ONLY] = "창 모드에서만 사용할 수 있습니다",
    [UI_MENU_PRESENT] = "화면 출력 방식", [UI_MENU_PRESENT_DESC] = "완성된 프레임이 화면에 표시되는 방식입니다.",
    [UI_MENU_PRESENT_MAILBOX] = "메일박스", [UI_MENU_PRESENT_VSYNC] = "수직 동기화", [UI_MENU_PRESENT_IMMEDIATE] = "즉시",
    [UI_MENU_PRESENT_IN_USE] = "사용 중: %s",
    [UI_MENU_GPU] = "그래픽 카드", [UI_MENU_GPU_DESC] = "게임을 그리는 Vulkan 장치입니다.",
    [UI_MENU_GPU_AUTO] = "자동",
    [UI_MENU_PERF_OVERLAY] = "성능 오버레이",
    [UI_MENU_PERF_OVERLAY_DESC] = "오른쪽 위에 프레임 레이트를 표시합니다. 전체는 그래프와 함께 프레임 시간, CPU, GPU, "
                                  "메모리 부하, 온도, 렌더 스레드 대기 시간을 추가로 표시합니다. F10으로 모드를 순환합니다.",
    [UI_MENU_PERF_OFF] = "끄기", [UI_MENU_PERF_FPS] = "FPS", [UI_MENU_PERF_FULL] = "전체",
    [UI_PERF_LINE_FPS] = "%.0f fps",
    [UI_MENU_GRAPHICS] = "화질",
    [UI_MENU_OUTPUT_RES] = "렌더링 해상도",
    [UI_MENU_ULTRAWIDE] = "울트라와이드",
    [UI_MENU_ULTRAWIDE_DESC] = "21:9 또는 32:9 화면에 맞게 카메라의 가로 시야를 넓힙니다. 메뉴는 비율을 유지합니다. "
                              "끄면 16:9 시야를 사용합니다. 변경하면 높이가 가장 가까운 해상도가 선택됩니다. 재시작해야 적용됩니다.",
    [UI_MENU_WIDE_RES] = "렌더링 해상도",
    [UI_MENU_WIDE_RES_DESC] = "게임이 창이나 화면에 맞게 확대/축소하기 전에 렌더링하는 해상도입니다. "
                             "해상도가 높을수록 디테일이 좋아지지만 GPU 시간과 비디오 메모리를 더 사용합니다. "
                             "울트라와이드를 켜면 21:9와 32:9를 선택할 수 있습니다. 재시작해야 적용됩니다.",
    [UI_MENU_HUD_POSITION] = "HUD 위치",
    [UI_MENU_HUD_POSITION_DESC] = "모서리 표시를 화면 가장자리에 두거나, 가운데 16:9 영역 안에 둡니다. "
                                 "메뉴와 조준 표시는 가운데에 유지됩니다. 재시작해야 적용됩니다.",
    [UI_MENU_HUD_EDGES] = "화면 가장자리", [UI_MENU_HUD_CENTERED] = "가운데 (16:9)",
    [UI_MENU_WIDE_DISABLED] = "먼저 울트라와이드를 켜세요",
    [UI_MENU_OUTPUT_RES_DESC] = "게임이 화면과 인터페이스를 그리는 크기입니다. 1920 x 1080보다 크면 비디오 메모리와 "
                                "GPU 시간이 더 필요합니다. 더 넓은 크기(21:9)는 카메라 시야를 넓히고 HUD를 화면 모서리로 "
                                "옮기며, 더 높은 크기(16:10, Steam Deck)는 HUD를 위아래 가장자리로 옮깁니다.",
    [UI_MENU_RENDER_SCALE] = "3D 렌더링 배율",
    [UI_MENU_RENDER_SCALE_DESC] = "게임 해상도 대비 3D 장면의 크기입니다. 게임의 FSR이 이를 해당 크기로 확대합니다. "
                                  "100 %를 넘으면 계단 현상이 줄어들지만(슈퍼샘플링) GPU 부하가 큽니다.",
    [UI_MENU_RENDER_SCALE_GAME] = "게임 기본 (83 %)",
    [UI_MENU_RAIDERS_RES] = "해상도",
    [UI_MENU_RAIDERS_RES_DESC] = "게임이 렌더링하는 출력으로, 해당 해상도의 TV에 연결된 Switch 2와 같습니다. TV 모드는 "
                                 "1920 x 1080, 2560 x 1440 또는 3840 x 2160, 휴대 모드는 1080p에서 1280 x 720, 그 이상에서 "
                                 "1920 x 1080입니다. 높을수록 비디오 메모리와 GPU 시간이 더 필요합니다.",
    [UI_MENU_UPSCALER] = "창 크기에 맞춘 확대",
    [UI_MENU_UPSCALER_DESC] = "창이 게임 해상도보다 클 때 화면을 확대하는 방식입니다. FSR은 바이리니어보다 가장자리와 "
                              "디테일을 더 선명하게 유지합니다.",
    [UI_MENU_UPSCALER_FSR] = "AMD FSR 1", [UI_MENU_UPSCALER_BILINEAR] = "바이리니어",
    [UI_MENU_ASPECT] = "창에 화면 맞추기",
    [UI_MENU_ASPECT_DESC] = "창의 비율이 게임 해상도와 다를 때(21:9 화면의 16:9 영상): 검은 여백으로 영상 비율을 "
                            "유지하거나, 창 전체로 늘립니다. 카메라 시야는 넓어지지 않습니다.",
    [UI_MENU_ASPECT_FIT] = "검은 여백", [UI_MENU_ASPECT_STRETCH] = "늘이기",
    [UI_MENU_FOV] = "시야각",
    [UI_MENU_FOV_DESC] = "카메라가 보여 주는 세계의 범위로, 게임 기본값에 대한 백분율입니다. 게임 버전 1.4.3 전용입니다.",
    [UI_MENU_SHARPNESS] = "FSR 선명도",
    [UI_MENU_SHARPNESS_DESC] = "FSR 확대 후의 선명화입니다. 0이면 꺼집니다.",
    [UI_MENU_SMAA] = "안티앨리어싱 (SMAA)",
    [UI_MENU_SMAA_DESC] = "창 크기에 맞춰 확대한 뒤, 이 메뉴를 그리기 전에 최종 화면의 계단 현상을 줄입니다. "
                          "HUD에도 적용됩니다. 품질이 높을수록 GPU 시간이 더 듭니다.",
    [UI_MENU_SMAA_LOW] = "낮음", [UI_MENU_SMAA_MEDIUM] = "중간", [UI_MENU_SMAA_HIGH] = "높음",
    [UI_MENU_COLOR] = "색상",
    [UI_MENU_COLOR_PRESET] = "색상 프로필",
    [UI_MENU_COLOR_PRESET_DESC] = "아래 네 가지 값을 설정합니다. 선명함: 대비와 채도를 높이고 어두운 부분을 조금 밝힙니다"
                                  "(OLED 화면용). 클래리티: 대비, 채도, 밝기를 조금 높입니다. 값을 움직이면 프로필이 "
                                  "사용자 지정이 됩니다. HUD는 화면과 함께 바뀌지만 이 메뉴는 바뀌지 않습니다.",
    [UI_MENU_COLOR_DEFAULT] = "게임 기본", [UI_MENU_COLOR_VIVID] = "선명함 (OLED)", [UI_MENU_COLOR_CLARITY] = "클래리티",
    [UI_MENU_COLOR_CUSTOM] = "사용자 지정",
    [UI_MENU_COLOR_BRIGHTNESS] = "밝기", [UI_MENU_COLOR_BRIGHTNESS_DESC] = "모든 색에 더해집니다. 0이면 그대로입니다.",
    [UI_MENU_COLOR_CONTRAST] = "대비",
    [UI_MENU_COLOR_CONTRAST_DESC] = "색을 중간 회색에서 멀어지게(100 % 초과) 또는 가까워지게 합니다. 100 %면 그대로입니다.",
    [UI_MENU_COLOR_SATURATION] = "채도",
    [UI_MENU_COLOR_SATURATION_DESC] = "회색 대비 색의 강도입니다. 100 %면 그대로입니다.",
    [UI_MENU_COLOR_GAMMA] = "감마",
    [UI_MENU_COLOR_GAMMA_DESC] = "1보다 작으면 어두운 톤과 중간 톤이 밝아지고, 1보다 크면 어두워집니다. 1이면 그대로입니다.",
    [UI_MENU_HDR_GROUP] = "HDR (실험적)",
    [UI_MENU_HDR] = "HDR 출력",
    [UI_MENU_HDR_DESC] = "실험적: HDR 디스플레이에서 게임의 SDR 화면의 밝은 부분을 끌어올립니다. Windows에서 HDR(\"HDR 사용\")을 "
                         "켜거나 색 관리를 지원하는 Wayland 컴포지터가 필요하며, 없으면 SDR로 유지됩니다. 재시작 후 적용됩니다.",
    [UI_MENU_HDR_NATIVE] = "게임 자체 HDR",
    [UI_MENU_HDR_NATIVE_DESC] = "HDR 출력이 켜져 있을 때: 끌어올린 SDR 화면 대신 콘솔처럼 게임이 자체 HDR 화면을 "
                                "렌더링하며, 밝은 부분은 최대 밝기까지 표시됩니다(게임은 1400니트에서 멈춤). 이 모드에서는 "
                                "FSR, SMAA, 색상 조정이 꺼집니다. 재시작 후 적용됩니다.",
    [UI_MENU_HDR_PEAK] = "최대 밝기",
    [UI_MENU_HDR_PEAK_DESC] = "디스플레이의 최대 밝기(니트)입니다. 가장 밝은 부분이 여기서 멈춥니다.",
    [UI_MENU_HDR_GAME] = "게임 밝기",
    [UI_MENU_HDR_GAME_DESC] = "화면 흰색(페이퍼 화이트)의 니트 값입니다. 203이 HDR 기준이며, 밝은 방에서는 높이세요.",
    [UI_MENU_HDR_UI] = "메뉴 밝기",
    [UI_MENU_HDR_UI_DESC] = "이 메뉴 흰색의 니트 값입니다. 게임의 HUD는 화면의 일부이므로 게임 밝기를 따릅니다.",

    [UI_MENU_FPS_LIMIT] = "프레임 레이트 제한",
    [UI_MENU_DKB_FPS_LIMIT_DESC] = "게임 속도를 실제 시간에 맞추기가 켜져 있으면 변경이 바로 적용됩니다. 60을 넘으려면 "
                                 "그 실험적 설정과 충분한 CPU 및 GPU 성능이 필요합니다. 무제한은 실험적입니다.",
    [UI_MENU_FPS_LIMIT_DESC] = "게임이 초당 그리는 최대 프레임 수입니다. 60 초과는 실험적이며 게임 속도를 실제 시간에 "
                               "맞추기가 필요합니다. 무제한은 실험적입니다.",
    [UI_MENU_FPS_30] = "30 FPS", [UI_MENU_FPS_40] = "40 FPS", [UI_MENU_FPS_45] = "45 FPS",
    [UI_MENU_FPS_60] = "60 FPS", [UI_MENU_FPS_90] = "90 FPS", [UI_MENU_FPS_120] = "120 FPS", [UI_MENU_FPS_144] = "144 FPS",
    [UI_MENU_FPS_DISPLAY] = "모니터 주사율", [UI_MENU_FPS_DISPLAY_HZ] = "모니터 주사율 (%.0f Hz)",
    [UI_MENU_FPS_UNLIMITED] = "무제한",
    [UI_MENU_FPS_NEEDS_REALTIME] = "게임 속도를 실제 시간에 맞추기 필요(재시작): 60 FPS 미만 및 초과 제한",
    [UI_MENU_REALTIME] = "게임 속도를 실제 시간에 맞추기",
    [UI_MENU_REALTIME_DESC] = "게임을 실제 시간 기준으로 진행하여, 느린 프레임이 게임을 느리게 만들지 않습니다.",
    [UI_MENU_DKB_REALTIME_DESC] = "실험적이며 기본값은 꺼짐입니다. 측정한 프레임 시간에 맞춰 게임 타이밍을 조정합니다. "
                                "카메라, 물리, 컷신, 오디오는 아직 검증이 필요합니다. 재시작해야 합니다.",
    [UI_MENU_CLOTH_REALTIME] = "옷감도 실제 시간 기준",
    [UI_MENU_CLOTH_REALTIME_DESC] = "옷감과 머리카락도 실제 시간 기준으로 진행합니다.",
    [UI_MENU_HIGHFPS_FIX] = "고프레임 수정",
    [UI_MENU_HIGHFPS_FIX_DESC] = "초당 30프레임을 넘을 때 이상하게 동작하는 물리와, 60을 넘을 때 투명하거나 보이지 않는 물체를 수정합니다.",
    [UI_MENU_GAME_LANG] = "게임 언어", [UI_MENU_GAME_LANG_DESC] = "게임의 텍스트와 음성 언어입니다.",
    [UI_MENU_GAME_LANG_AUTO] = "자동",
    [UI_MENU_OPERATION_MODE] = "콘솔 모드",
    [UI_MENU_OPERATION_MODE_DESC] = "게임이 TV 모드로 실행 중인지 휴대 모드로 실행 중인지를 정합니다.",
    [UI_MENU_MODE_DOCKED] = "TV 모드", [UI_MENU_MODE_HANDHELD] = "휴대 모드",
    [UI_MENU_ASSUME_FOCUS] = "포커스가 없어도 계속 실행",
    [UI_MENU_ASSUME_FOCUS_DESC] = "켜면(기본값) 다른 창을 사용하는 동안에도 게임이 계속 실행되며, 창이 다시 앞으로 올 때까지 "
                                  "키보드 입력은 무시됩니다. 끄면 Switch에서 HOME을 눌렀을 때처럼 창이 포커스를 잃으면 "
                                  "일시 정지됩니다. 이렇게 중단된 로딩은 영영 멈출 수 있습니다.",

    [UI_MENU_VOLUME] = "음량", [UI_MENU_VOLUME_DESC] = "게임의 전체 음량입니다.",
    [UI_MENU_AUDIO_CHANNELS] = "스피커",
    [UI_MENU_AUDIO_CHANNELS_DESC] = "스테레오 또는 5.1 서라운드 출력입니다. 자동은 기본 출력 장치가 6채널 이상이면 "
                                    "5.1을 선택합니다. 재시작 후 적용됩니다.",
    [UI_MENU_CHANNELS_AUTO] = "자동", [UI_MENU_CHANNELS_STEREO] = "스테레오", [UI_MENU_CHANNELS_51] = "5.1 서라운드",

    [UI_MENU_MOTION] = "모션 컨트롤", [UI_MENU_MOTION_DESC] = "컨트롤러의 자이로 센서로 조준합니다.",
    [UI_MENU_RUMBLE_STRENGTH] = "진동 세기",
    [UI_MENU_RUMBLE_STRENGTH_DESC] = "컨트롤러가 진동하는 세기입니다.",
    [UI_MENU_RUMBLE_CURVE] = "진동 곡선",
    [UI_MENU_RUMBLE_CURVE_DESC] = "값이 낮을수록 약한 진동이 강해집니다.",
    [UI_MENU_RUMBLE_TRIGGERS] = "트리거 진동 (Xbox)",
    [UI_MENU_RUMBLE_TRIGGERS_DESC] = "Xbox 컨트롤러의 트리거도 진동시킵니다.",
    [UI_MENU_PAD_LAYOUT] = "버튼 배치",
    [UI_MENU_PAD_LAYOUT_DESC] = "어느 버튼이 A인지 정합니다. Nintendo: Switch처럼 오른쪽 버튼. Xbox: Xbox 컨트롤러 표기처럼 "
                                "아래쪽 버튼(PlayStation 컨트롤러의 X). 자동은 Nintendo 표기 컨트롤러에는 Nintendo를, 그 밖에는 "
                                "Xbox를 선택합니다. Xbox 표시: A는 Nintendo처럼 오른쪽 버튼이고, 게임이 Xbox 표기를 보여 줍니다"
                                "(A 대신 B, B 대신 A, X 대신 Y, Y 대신 X). 표시는 재시작 후 바뀝니다. 글자는 게임 파일의 "
                                "일부이므로 조작에서 다시 지정한 버튼은 화면에 이전 글자로 표시됩니다.",
    [UI_MENU_PAD_LAYOUT_AUTO] = "자동", [UI_MENU_PAD_LAYOUT_NINTENDO] = "Nintendo (A 오른쪽)",
    [UI_MENU_PAD_LAYOUT_XBOX] = "Xbox (A 아래쪽)", [UI_MENU_PAD_LAYOUT_XBOX_PROMPTS] = "Xbox 표시 (A 오른쪽)",
    [UI_MENU_PAD_LAYOUT_IN_USE] = "사용 중: %s",
    [UI_MENU_PAD_LAYOUT_PROMPTS_MISSING] = "버튼 표시 파일 없음: 설치를 다시 실행하세요",
    [UI_MENU_PAD_LAYOUT_PROMPTS_RESTART] = "버튼 표시는 재시작 후 바뀝니다",
    [UI_MENU_STICK_DEADZONE] = "스틱 데드존",
    [UI_MENU_STICK_DEADZONE_DESC] = "게임이 인식하기 전까지 게임패드 스틱이 움직여야 하는 거리입니다. 캐릭터나 카메라가 저절로 움직이면 높이세요.",
    [UI_MENU_MOUSE_CAMERA] = "마우스 직접 카메라",
    [UI_MENU_MOUSE_CAMERA_DESC] = "컨트롤러식 가속이나 스무딩 없이 마우스 움직임으로 일반 추적 카메라를 직접 회전합니다. "
                                "다른 카메라 모드는 컨트롤러식 마우스 입력을 사용합니다. 끄면 모든 곳에서 컨트롤러식 입력으로 돌아갑니다.",
    [UI_MENU_MOUSE_SENS] = "마우스 카메라 감도",
    [UI_MENU_MOUSE_SENS_DESC] = "마우스로 카메라를 돌리는 속도입니다.",

    [UI_BINDS_HELP] = "칸을 선택하고 A 또는 Enter를 누른 뒤, 키나 마우스 버튼 또는 컨트롤러 버튼을 누르세요. Delete 또는 "
                      "X로 칸을 비웁니다. 이미 사용 중인 키는 여기로 옮겨지고, 컨트롤러 버튼은 대체되는 버튼과 서로 바뀝니다.",
    [UI_BINDS_COL_KEY1] = "키 1", [UI_BINDS_COL_KEY2] = "키 2", [UI_BINDS_COL_PAD] = "컨트롤러",
    [UI_BINDS_GROUP_BUTTONS] = "버튼", [UI_BINDS_GROUP_MOVE] = "이동", [UI_BINDS_GROUP_CAMERA] = "카메라",
    [UI_BINDS_PRESS_KEY] = "%s에 지정할 키나 마우스 버튼을 누르세요",
    [UI_BINDS_PRESS_PAD] = "%s에 지정할 컨트롤러 버튼을 누르세요",
    [UI_BINDS_CANCEL_HINT] = "Esc로 취소하거나 %d초 기다리세요",
    [UI_BINDS_TAKEN] = "%s: %s에서 가져왔습니다", [UI_BINDS_SWAPPED] = "%s와(과) %s를 서로 바꿨습니다",
    [UI_BINDS_RESET] = "모든 버튼 초기화",
    [UI_BINDS_RESET_DESC] = "기본 키와 컨트롤러 버튼으로 되돌립니다. 위의 버튼 배치가 다시 적용됩니다. 다시 지정한 컨트롤러 버튼은 이 초기화 전까지 그대로 유지됩니다.",
    [UI_BIND_A] = "A", [UI_BIND_B] = "B", [UI_BIND_X] = "X", [UI_BIND_Y] = "Y",
    [UI_BIND_LCLICK] = "왼쪽 스틱 누르기", [UI_BIND_RCLICK] = "오른쪽 스틱 누르기", [UI_BIND_L] = "L",
    [UI_BIND_R] = "R", [UI_BIND_ZL] = "ZL", [UI_BIND_ZR] = "ZR", [UI_BIND_PLUS] = "+", [UI_BIND_MINUS] = "-",
    [UI_BIND_LEFT] = "방향 버튼 왼쪽", [UI_BIND_UP] = "방향 버튼 위", [UI_BIND_RIGHT] = "방향 버튼 오른쪽",
    [UI_BIND_DOWN] = "방향 버튼 아래",
    [UI_BIND_LSTICK_LEFT] = "왼쪽으로 이동", [UI_BIND_LSTICK_UP] = "앞으로 이동", [UI_BIND_LSTICK_RIGHT] = "오른쪽으로 이동",
    [UI_BIND_LSTICK_DOWN] = "뒤로 이동",
    [UI_BIND_RSTICK_LEFT] = "카메라 왼쪽", [UI_BIND_RSTICK_UP] = "카메라 위", [UI_BIND_RSTICK_RIGHT] = "카메라 오른쪽",
    [UI_BIND_RSTICK_DOWN] = "카메라 아래",
    [UI_BIND_DESC_A] = "상호작용, 줍기, 대화.", [UI_BIND_DESC_B] = "달리기, 취소, 뒤로.",
    [UI_BIND_DESC_X] = "점프.", [UI_BIND_DESC_Y] = "공격.", [UI_BIND_DESC_LCLICK] = "웅크리기.",
    [UI_BIND_DESC_RCLICK] = "1인칭 시점.", [UI_BIND_DESC_L] = "능력.",
    [UI_BIND_DESC_R] = "무기 던지기.", [UI_BIND_DESC_ZL] = "방패, 주목.", [UI_BIND_DESC_ZR] = "활.",
    [UI_BIND_DESC_PLUS] = "메뉴, 소지품.", [UI_BIND_DESC_MINUS] = "지도.",
    [UI_BIND_DESC_DPAD] = "빠른 선택.",
    [UI_BIND_DESC_MOVE] = "왼쪽 스틱처럼 캐릭터를 움직입니다. 컨트롤러의 왼쪽 스틱도 항상 작동합니다.",
    [UI_BIND_DESC_CAMERA] = "오른쪽 스틱처럼 카메라를 돌립니다. 마우스와 컨트롤러의 오른쪽 스틱도 항상 작동합니다.",
    [UI_KEY_SPACE] = "스페이스", [UI_KEY_TAB] = "Tab", [UI_KEY_LSHIFT] = "왼쪽 Shift", [UI_KEY_LCTRL] = "왼쪽 Ctrl",
    [UI_KEY_UP] = "위쪽 화살표", [UI_KEY_DOWN] = "아래쪽 화살표", [UI_KEY_LEFT] = "왼쪽 화살표",
    [UI_KEY_RIGHT] = "오른쪽 화살표",
    [UI_KEY_MOUSE_L] = "마우스 왼쪽", [UI_KEY_MOUSE_R] = "마우스 오른쪽", [UI_KEY_MOUSE_M] = "마우스 가운데",
    [UI_KEY_MOUSE_X1] = "마우스 4", [UI_KEY_MOUSE_X2] = "마우스 5",
    [UI_KEY_ENTER] = "Enter", [UI_KEY_BACKSPACE] = "Backspace", [UI_KEY_RSHIFT] = "오른쪽 Shift",
    [UI_KEY_RCTRL] = "오른쪽 Ctrl", [UI_KEY_LALT] = "Alt", [UI_KEY_CAPSLOCK] = "Caps Lock", [UI_KEY_INSERT] = "Insert",
    [UI_KEY_DELETE] = "Delete", [UI_KEY_HOME] = "Home", [UI_KEY_END] = "End", [UI_KEY_PAGEUP] = "Page Up",
    [UI_KEY_PAGEDOWN] = "Page Down", [UI_KEY_NUM_FMT] = "숫자패드 %s",
    [UI_PAD_SOUTH] = "아래쪽 버튼", [UI_PAD_EAST] = "오른쪽 버튼", [UI_PAD_WEST] = "왼쪽 버튼",
    [UI_PAD_NORTH] = "위쪽 버튼", [UI_PAD_LSTICK] = "왼쪽 스틱", [UI_PAD_RSTICK] = "오른쪽 스틱",
    [UI_PAD_L] = "왼쪽 범퍼", [UI_PAD_R] = "오른쪽 범퍼", [UI_PAD_START] = "Start", [UI_PAD_BACK] = "Back",
    [UI_PAD_DPAD_LEFT] = "방향 버튼 왼쪽", [UI_PAD_DPAD_UP] = "방향 버튼 위", [UI_PAD_DPAD_RIGHT] = "방향 버튼 오른쪽",
    [UI_PAD_DPAD_DOWN] = "방향 버튼 아래", [UI_PAD_ZL] = "왼쪽 트리거", [UI_PAD_ZR] = "오른쪽 트리거",
    [UI_PAD_LPADDLE1] = "왼쪽 뒷면 버튼", [UI_PAD_RPADDLE1] = "오른쪽 뒷면 버튼",
    [UI_PAD_LPADDLE2] = "왼쪽 뒷면 버튼 2", [UI_PAD_RPADDLE2] = "오른쪽 뒷면 버튼 2",
    [UI_PAD_CROSS] = "엑스", [UI_PAD_CIRCLE] = "동그라미", [UI_PAD_SQUARE] = "네모", [UI_PAD_TRIANGLE] = "세모",

    [UI_MENU_MENU_LANG] = "메뉴 언어", [UI_MENU_MENU_LANG_DESC] = "이 메뉴와 오버레이의 언어입니다.",
    [UI_MENU_MENU_LANG_AUTO] = "자동 (Windows 설정)",
    [UI_MENU_THEME] = "메뉴 테마", [UI_MENU_THEME_DK] = "Donkey Kong", [UI_MENU_THEME_CLASSIC] = "클래식",
    [UI_MENU_THEME_DESC] = "이 메뉴의 모양입니다: Donkey Kong(나무껍질과 바나나 노랑) 또는 클래식 짙은 초록과 금색.",

    [UI_MENU_ADV_WARNING] = "이 설정은 테스트와 문제 해결용입니다. 이유를 알 때만 변경하세요.",
    [UI_MENU_GROUP_SCHED] = "스케줄링", [UI_MENU_GROUP_SHADERS] = "셰이더와 파이프라인",
    [UI_MENU_GROUP_TEXMEM] = "텍스처와 메모리", [UI_MENU_GROUP_FIXES] = "렌더링 수정",
    [UI_MENU_GROUP_VULKAN] = "Vulkan", [UI_MENU_GROUP_DIAG] = "진단",
    [UI_MENU_ASYNC_PREEMPT] = "가상 코어 선점",
    [UI_MENU_ASYNC_PREEMPT_DESC] = "같은 가상 코어의 다른 게임 스레드를 중단시킬 수 있게 합니다.",
    [UI_MENU_PREEMPT_OFF] = "끄기", [UI_MENU_PREEMPT_ON] = "켜기", [UI_MENU_PREEMPT_INHERIT] = "상속만",
    [UI_MENU_REPLAY_ASYNC] = "GPU 프레임 준비를 별도 스레드에서",
    [UI_MENU_REPLAY_ASYNC_DESC] = "게임 스레드 대신 별도 스레드에서 GPU용 프레임을 준비합니다. 최대 1프레임의 지연이 추가됩니다.",
    [UI_MENU_TIMER_1MS] = "1 ms Windows 타이머",
    [UI_MENU_TIMER_1MS_DESC] = "더 안정적인 프레임 간격을 위해 시스템 타이머 해상도를 1 ms로 높입니다.",
    [UI_MENU_PIPE_WAIT] = "대기 중인 그리기 처리",
    [UI_MENU_PIPE_WAIT_DESC] = "기본값은 대기 중인 그리기용 전용 작업자를 둔 현재 스케줄링을 유지합니다. 이전 방식(그리기)은 예전 스케줄링을 사용합니다.",
    [UI_MENU_PIPE_WAIT_DEFAULT] = "기본값", [UI_MENU_PIPE_WAIT_DRAW] = "이전 방식 (그리기)",
    [UI_MENU_PIPE_SKIP] = "파이프라인 빌드 중 건너뛸 그리기",
    [UI_MENU_PIPE_SKIP_DESC] = "파이프라인을 기다리지 않고 버릴 그리기를 정합니다.",
    [UI_MENU_PIPE_SKIP_STRICT] = "엄격", [UI_MENU_PIPE_SKIP_ANY] = "모두",
    [UI_MENU_PIPE_SKIP_FULLSCREEN] = "전체 화면 패스",
    [UI_MENU_LIB_PREBUILD] = "스테이지 라이브러리 사전 빌드",
    [UI_MENU_LIB_PREBUILD_DESC] = "셰이더 스테이지 라이브러리를 백그라운드에서 미리 빌드합니다.",
    [UI_MENU_LIB_PREBUILD_THREADS] = "스테이지 라이브러리 사전 빌드 스레드",
    [UI_MENU_LIB_PREBUILD_THREADS_DESC] = "사전 빌드에 쓰는 스레드 수입니다. 자동은 CPU에 맞춰 정합니다.",
    [UI_MENU_PIPE_PREBUILD] = "파이프라인 사전 빌드",
    [UI_MENU_PIPE_PREBUILD_DESC] = "알려진 파이프라인을 미리 빌드합니다.",
    [UI_MENU_PREWARM_XLATE] = "번역기 예열",
    [UI_MENU_PREWARM_XLATE_DESC] = "시작할 때 알려진 셰이더를 번역하는 스레드입니다. 자동은 CPU 스레드의 절반(최대 16)을 사용합니다.",
    [UI_MENU_GPL] = "그래픽 파이프라인 라이브러리",
    [UI_MENU_GPL_DESC] = "Vulkan 파이프라인 라이브러리 확장을 사용합니다. 드라이버에 문제가 있으면 끄세요.",
    [UI_MENU_ASTC] = "ASTC 텍스처 디코딩", [UI_MENU_ASTC_DESC] = "압축된 ASTC 텍스처를 디코딩하는 위치입니다.",
    [UI_MENU_ASTC_AUTO] = "자동", [UI_MENU_ASTC_GPU] = "GPU 셰이더", [UI_MENU_ASTC_NATIVE] = "네이티브",
    [UI_MENU_ASTC_CPU] = "CPU",
    [UI_MENU_MEMORY] = "GPU 메모리 모드", [UI_MENU_MEMORY_DESC] = "게임 메모리를 GPU와 공유하는 방식입니다.",
    [UI_MENU_MEMORY_AUTO] = "자동", [UI_MENU_MEMORY_IMPORT] = "가져오기", [UI_MENU_MEMORY_MIRROR] = "미러",
    [UI_MENU_MEMORY_VERIFY] = "검증",
    [UI_MENU_TEX_ALIAS] = "텍스처 별칭", [UI_MENU_TEX_ALIAS_DESC] = "사용 중인 텍스처의 메모리 위에 만든 새 텍스처가 비어 있지 않고 그 내용으로 시작합니다.",
    [UI_MENU_TEX_VRAM_PRESSURE] = "텍스처 메모리 한도", [UI_MENU_TEX_VRAM_PRESSURE_DESC] = "게임이 해제한 텍스처를 버리기 시작하는 비디오 메모리 비율입니다(다시 사용할 때 저장소에서 복원됩니다). 0이면 유지합니다.",
    [UI_MENU_ASTC_BC] = "ASTC를 BC3로 압축", [UI_MENU_ASTC_BC_DESC] = "GPU 디코더 사용 시: 디코딩한 ASTC 텍스처를 BC3로 다시 인코딩하여 비디오 메모리를 4분의 1로 줄입니다. 4 GB 이하 GPU용이며, 텍스처가 약간 뭉개져 보입니다.",
    [UI_MENU_LOD_GUARD] = "LOD 보호", [UI_MENU_LOD_GUARD_DESC] = "GPU가 읽기 전에 게임이 세부 수준(LOD) 데이터를 다시 쓰는 것을 기다려, 물체가 한 프레임 동안 엉뚱한 곳에 번쩍이거나 먼 풍경이 평평해지는 것을 막습니다.",
    [UI_MENU_FIX_C0] = "상수 버퍼 수정",
    [UI_MENU_FIX_C0_DESC] = "0을 읽는 대신 드라이버 예약 상수 뱅크 c0(저장소 크기, 디스패치 크기, 점 크기, 기본 인스턴스)을 채웁니다.",
    [UI_MENU_FIX_ZERO] = "새 GPU 메모리 0으로 채우기",
    [UI_MENU_FIX_ZERO_DESC] = "Switch 메모리처럼 새 GPU 메모리 풀을 0으로 채운 상태로 시작합니다.",
    [UI_MENU_LIST_RESTART] = "리스트의 프리미티브 재시작",
    [UI_MENU_LIST_RESTART_DESC] = "Switch처럼 삼각형 리스트 그리기에도 게임의 프리미티브 재시작을 적용합니다. 없으면 재시작 인덱스가 정점으로 그려져, 예를 들어 지도에 길게 늘어난 삼각형이 생깁니다. primitiveTopologyListRestart를 지원하는 GPU가 필요합니다.",
    [UI_MENU_BIND_SKIP] = "반복 바인딩 건너뛰기",
    [UI_MENU_BIND_SKIP_DESC] = "최적화: 바뀌지 않은 바인딩을 건너뜁니다.",
    [UI_MENU_SHADER_MEMO] = "셰이더 메모", [UI_MENU_SHADER_MEMO_DESC] = "최적화: 셰이더 조회 결과를 기억합니다.",
    [UI_MENU_PIPE_MEMO] = "파이프라인 메모", [UI_MENU_PIPE_MEMO_DESC] = "최적화: 상태가 반복되면 이전 그리기의 파이프라인을 재사용합니다.",
    [UI_MENU_TEX_MEMO] = "텍스처 바인딩 메모", [UI_MENU_TEX_MEMO_DESC] = "최적화: 그리기 사이에 텍스처 디스크립터를 기억합니다.",
    [UI_MENU_DESC_TEMPLATE] = "디스크립터 템플릿",
    [UI_MENU_DESC_TEMPLATE_DESC] = "최적화: 디스크립터를 한꺼번에 업데이트합니다.",
    [UI_MENU_PUSH_SKIP] = "반복 푸시 상수 건너뛰기",
    [UI_MENU_PUSH_SKIP_DESC] = "최적화: 바뀌지 않은 푸시 상수를 건너뜁니다.",
    [UI_MENU_SPLIT_DRAWS] = "GPU 조기 제출",
    [UI_MENU_SPLIT_DRAWS_DESC] = "패스 사이에서 N번의 그리기마다 GPU 작업을 나누어 제출해 GPU가 더 일찍 시작하게 합니다. 0이면 꺼집니다.",
    [UI_MENU_VK_VALIDATION] = "검증 레이어",
    [UI_MENU_VK_VALIDATION_DESC] = "모든 Vulkan 호출을 검사하며 게임이 크게 느려집니다.",
    [UI_MENU_VK_VALIDATION_MISSING] = "검증 레이어가 설치되어 있지 않습니다.",
    [UI_MENU_VK_DYN_VERTEX] = "동적 정점 상태",
    [UI_MENU_VK_DYN_VERTEX_DESC] = "동적 정점 입력을 강제로 켜거나 끕니다. 기본값은 GPU에 따라 다릅니다.",
    [UI_MENU_VK_DYN_A2C] = "동적 알파 투 커버리지",
    [UI_MENU_VK_DYN_A2C_DESC] = "동적 알파 투 커버리지를 강제로 켜거나 끕니다. 기본값: RDNA 2 이하용 AMD 드라이버에서는 "
                                "캐릭터가 검게 그려지므로 꺼짐, 그 밖에서는 켜짐.",
    [UI_MENU_VK_SUBGROUP_PIN] = "서브그룹 크기 고정", [UI_MENU_VK_SUBGROUP_PIN_DESC] = "셰이더 서브그룹 크기를 고정합니다.",
    [UI_MENU_TRACE] = "성능 로그", [UI_MENU_TRACE_DESC] = "프레임 시간을 파일에 기록합니다.",
    [UI_MENU_FLIGHT] = "플라이트 레코더",
    [UI_MENU_FLIGHT_DESC] = "짧은 기록을 보관하다가 프레임이 멈추면 저장합니다.",
    [UI_MENU_FLIGHT_SPIKE_MS] = "멈춤 기준 (ms)",
    [UI_MENU_FLIGHT_SPIKE_MS_DESC] = "멈춤으로 간주하는 프레임 시간입니다.",
    [UI_MENU_INPUT_RECORD] = "입력 기록",
    [UI_MENU_INPUT_RECORD_DESC] = "버그 보고를 위해 입력을 파일에 기록합니다.",
    [UI_MENU_OPEN_LOG] = "로그 폴더 열기", [UI_MENU_OPEN_LOG_DESC] = "로그 파일이 있는 폴더를 표시합니다.",
    [UI_MENU_COPY_INFO] = "시스템 정보 복사",
    [UI_MENU_COPY_INFO_DESC] = "GPU, 드라이버, CPU, RAM 및 변경한 설정을 클립보드에 복사합니다.",
    [UI_MENU_COPY_INFO_DONE] = "복사됨", [UI_MENU_ACTION_FAILED] = "실패했습니다",
    [UI_MENU_RESET] = "모든 설정을 기본값으로 초기화", [UI_MENU_RESET_DESC] = "모든 탭에 적용됩니다.",
    [UI_MENU_RESET_CONFIRM] = "모든 설정을 기본값으로 초기화할까요?", [UI_MENU_RESET_YES] = "초기화",
    [UI_MENU_RESET_NO] = "취소",

    [UI_MENU_OV_FPS] = "%.0f FPS", [UI_MENU_OV_FRAME_MS] = "%.1f ms", [UI_MENU_OV_GAME_PCT] = "게임 %.0f%%",
    [UI_MENU_OV_MAX_MS] = "최대 %.1f ms",
    [UI_MENU_OV_GPU] = "GPU", [UI_MENU_OV_CPU] = "CPU", [UI_MENU_OV_RAM] = "RAM", [UI_MENU_OV_VRAM] = "VRAM",
    [UI_MENU_OV_TEMP] = "온도", [UI_MENU_OV_POWER] = "전력", [UI_MENU_OV_CLOCK] = "클럭",
    [UI_MENU_OV_NONE] = "-",
    [UI_MENU_OV_REPLAY] = "GPU 프레임 준비", [UI_MENU_OV_GPU_TIME] = "프레임당 GPU 사용",
    [UI_MENU_OV_PIPE_WAIT] = "파이프라인 대기",
    [UI_MENU_OV_SHADER_WAIT] = "셰이더 대기", [UI_MENU_OV_TEX_UPLOADS] = "텍스처 업로드",
    [UI_MENU_OV_FMT_PCT] = "%.0f%%", [UI_MENU_OV_FMT_TEMP] = "%.0f °C", [UI_MENU_OV_FMT_POWER] = "%.0f W",
    [UI_MENU_OV_FMT_MHZ] = "%.0f MHz", [UI_MENU_OV_FMT_GB] = "%.1f GB", [UI_MENU_OV_FMT_GB_OF] = "/ %.1f GB",
    [UI_MENU_OV_GAME_GB] = "게임 %.1f GB", [UI_MENU_OV_FMT_MS] = "%.2f ms", [UI_MENU_OV_PER_FRAME] = "프레임당 %.1f",
    [UI_MENU_OV_WINDOW] = "창", [UI_MENU_OV_OUTPUT] = "게임 영상", [UI_MENU_OV_SCENE] = "3D 장면",
    [UI_MENU_OV_SCALING] = "확대/축소", [UI_MENU_OV_LIMIT] = "프레임 제한", [UI_MENU_OV_PRESENT] = "화면 출력",
    [UI_MENU_OV_FMT_SIZE] = "%ux%u", [UI_MENU_OV_FMT_SCENE] = "%ux%u, %.0f%%",
    [UI_MENU_OV_FMT_UPSCALE] = "%ux%u -> %ux%u",
    [UI_MENU_OV_NO_UPSCALER] = "업스케일러 없음",
    [UI_MENU_OV_FMT_FSR] = "AMD FSR 1, 선명도 %d%%", [UI_MENU_OV_SCALE_NONE] = "없음 (1:1)",
    [UI_MENU_OV_SCALE_BILINEAR] = "바이리니어",
};

#define SWKBD_ONLY(name, ...) static const char *const name[UI_TEXT_COUNT] = {SWKBD(__VA_ARGS__)}
SWKBD_ONLY(text_ja, "文字入力", "名前を入力してください", "ウマ", "OK", "キャンセル", "コントローラー: A = OK、B = キャンセル");
SWKBD_ONLY(text_fr, "Saisie de texte", "Entrez un nom", "Cheval", "OK", "Annuler", "Manette : A = OK, B = Annuler");
SWKBD_ONLY(text_de, "Texteingabe", "Gib einen Namen ein", "Pferd", "OK", "Abbrechen", "Controller: A = OK, B = Abbrechen");
SWKBD_ONLY(text_it, "Inserimento testo", "Inserisci un nome", "Cavallo", "OK", "Annulla", "Controller: A = OK, B = Annulla");
SWKBD_ONLY(text_es, "Introducir texto", "Introduce un nombre", "Caballo", "Aceptar", "Cancelar",
           "Mando: A = Aceptar, B = Cancelar");
SWKBD_ONLY(text_nl, "Tekstinvoer", "Voer een naam in", "Paard", "OK", "Annuleren", "Controller: A = OK, B = Annuleren");
SWKBD_ONLY(text_pt, "Inserir texto", "Introduz um nome", "Cavalo", "OK", "Cancelar", "Comando: A = OK, B = Cancelar");
SWKBD_ONLY(text_zh_hans, "文字输入", "请输入名字", "马儿", "确定", "取消", "手柄：A 确定，B 取消");
SWKBD_ONLY(text_zh_hant, "文字輸入", "請輸入名字", "馬兒", "確定", "取消", "手把：A 確定，B 取消");

static const Row rows[] = {
    {"en", text_en}, {"ja", text_ja}, {"fr", text_fr}, {"de", text_de}, {"it", text_it}, {"es", text_es},
    {"nl", text_nl}, {"pt", text_pt}, {"ru", text_ru}, {"zh-Hans", text_zh_hans}, {"zh-Hant", text_zh_hant},
    {"ko", text_ko},
};

const char *ui_text_lang(const char *code) {
    static const struct { const char *code, *lang; } map[] = {
        {"en-US", "en"}, {"en-GB", "en"}, {"fr-CA", "fr"}, {"es-419", "es"}, {"pt-BR", "pt"},
        {"zh-CN", "zh-Hans"}, {"zh-TW", "zh-Hant"},
    };
    for (size_t i = 0; i < sizeof map / sizeof *map; i++)
        if (!strcmp(code, map[i].code)) return map[i].lang;
    return code;
}

static const Row *find_row(const char *lang) {
    for (size_t i = 0; i < sizeof rows / sizeof *rows; i++)
        if (!strcmp(rows[i].lang, lang)) return &rows[i];
    return NULL;
}

int ui_text_defined(const char *lang, UiTextId id) {
    const Row *r = find_row(lang);
    return (unsigned)id < UI_TEXT_COUNT && r && r->text[id] && *r->text[id];
}

const char *ui_text_in(const char *lang, UiTextId id) {
    if ((unsigned)id >= UI_TEXT_COUNT) return "";
    return ui_text_defined(lang, id) ? find_row(lang)->text[id] : rows[0].text[id];
}

const char *ui_text(UiTextId id) { return ui_text_in(ui_text_lang(ov_language()), id); }

// The menu languages: those with a whole menu catalog above.
static const char *const menu_langs[] = {"en", "ru", "ko"};

// The menu language, re-read when cfg_version changes. TOTK_MENU_LANG "en", "ru" or "ko"; anything else (unset
// included) follows the system UI language: Russian for "ru", "ru-RU", ..., Korean for "ko", "ko-KR", ..., else English.
static const char *resolve_menu_lang(void) {
    enum { N = sizeof menu_langs / sizeof *menu_langs };
    const char *v = cfg_value("TOTK_MENU_LANG");
    for (size_t i = 0; i < N; i++)
        if (v && !strcmp(v, menu_langs[i])) return menu_langs[i];
    const char *sys = plat_ui_language();
    for (size_t i = 0; sys && i < N; i++) {
        size_t n = strlen(menu_langs[i]);
        if (!strncmp(sys, menu_langs[i], n) && (!sys[n] || sys[n] == '-')) return menu_langs[i];
    }
    return "en";
}

const char *menu_text(UiTextId id) {
    static _Atomic(const char *) lang = "en";
    static _Atomic unsigned seen = ~0u;
    unsigned v = cfg_version();
    if (v != atomic_load_explicit(&seen, memory_order_relaxed)) {
        atomic_store_explicit(&lang, resolve_menu_lang(), memory_order_relaxed);
        atomic_store_explicit(&seen, v, memory_order_relaxed);
    }
    return ui_text_in(atomic_load_explicit(&lang, memory_order_relaxed), id);
}
