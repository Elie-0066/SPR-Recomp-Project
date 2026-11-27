# Raiders manual mouse camera

Open SPR Setup and select a `.nup` or the ExeFS and complete RomFS of the update you want to build. Setup automatically enables the direct mouse camera when it recognizes the camera routines in the selected update. If the routines differ, Setup warns and builds with the original camera controls.

The generator uses SHA-256 fingerprints of the player camera update, pitch map and pose builder from the verified 65536 update. It ignores relocatable call and address fields while checking local control flow, registers and field offsets. It follows the camera's calls to both helpers and calculates the six patch sites and their branch destinations for the target layout. A successful match still needs a gameplay test on that update.

The manual orbit receives relative mouse motion on the next eligible player camera update. A sensitivity of 1 requests 0.064 degrees per count on either axis in the standing view. While the mapped ZL button is held, vertical mouse motion travels the same fraction of the native camera's pitch range as in the standing view. The swim view can have a shorter range above its neutral angle than below it. A sensitivity of 0 releases the mouse. Vertical inversion is independent of sensitivity.

## Camera path

Offsets below are relative to the start of the verified main module.

| Site | Role |
| --- | --- |
| 0x22e49a0 | Construction path registering the player's camera as `spl:PlayerCamera`. |
| 0x245ed40 | Player camera update. The native control gates precede the two axis calculations. The hook at 0x245ffac marks updates that reach the manual select with a permitted mode. |
| 0x24603ac | Reads an additional yaw angle in radians. The mouse contributes here before the game rotates its heading vector. The pad has its own upstream response. |
| 0x2460e70, 0x2461128, 0x24611b0 | Call sites for the game's pitch map at 0x2466510. They supply the current reference, posture and limit arguments. |
| 0x2461d24 | The ordinary pitch response interpolates the normalized orbit coordinate. A solved mouse step uses its target in the current update. |
| 0x2462d24, 0x2462d3c | The pose builder at 0x245d750 evaluates the ordinary and alternate geometry using the resulting coordinate. Nonzero explicit pose requests can select another builder. |

The three pitch angles in the ordinary pose define its lower endpoint, neutral angle and upper endpoint. The guest input angle maps nonlinearly onto this range. The hook inverts the native pitch map on its regular branch to reach the requested pose angle or swim orbit coordinate. This keeps the stored input and the camera pose in agreement when motion stops. The native reference bounds, vertical adjustment and downstream view construction remain active. Requests outside the regular reference branch use the game's update.

The mouse adds no time multiplier, dead zone, smoothing or speed cap. It samples both raw axes once and applies the same scale during movement. Other sources of motion include the gamepad, native recentering, camera mode transitions and contact with scenery. Projectiles, the impact marker, collision queries and the renderer use their existing code.

## Cursor and controls

The manual camera captures the cursor while gameplay has focus. When the camera stops updating, a watchdog releases capture after five presents. A new capture discards pending motion. Mapped mouse buttons work inside the focused game window while the cursor is free, including on the title screen. The settings menu and text input block game clicks, and a click that activates the window only gives it focus.

F1 opens the controls, including mouse sensitivity and vertical inversion.

## Rebuild and check

Use the repository Python environment, the target ExeFS and RomFS, the matching installer manifest and the Windows toolchain described in the build guide. Run from the source root. The paths in angle brackets are local inputs.

```text
python recomp/gen.py --exefs <exefs> --out generated/raiders --raiders-mouse
python tools/build_game.py --version raiders --expected <expected-raiders.json> --gen generated/raiders --out build/native --exefs <exefs> --romfs <romfs> --profile <test-profile> --deps <deps> --toolchain <llvm> --ninja <ninja.exe> --exe spr-recomp --full-build
python tests/raiders_camera_test.py --exefs <65536-exefs> --deps <deps> --cc <clang.exe>
```

The regression test runs the real input and camera hooks with the locally recompiled native pitch map. It checks distance, reversals, both end stops, changes in posture and vertical range, inversion, suspended input, and cursor ownership. It simulates the window and manual camera entry. Gameplay checks still include wall contact, impact marker alignment, movement while firing, transitions, the pause screen and Alt+Tab on the actual window.

An independent check fed ten consecutive pitch updates into the original ARM64 pose builder with the standard pose parameters. Elevation measured from its eye and target matched the requested cumulative angle to 0.001 degrees, including reversal at each end stop. This check used only numerical coordinates.
