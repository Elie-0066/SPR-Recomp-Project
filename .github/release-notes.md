SPR Recomp: a static recompilation of *Splatoon Raiders* (Switch 2) to a native Windows and Linux executable with a Vulkan renderer. It is an experimental fork of DKB Recomp (itself built on TotK Recomp).

## New in 0.2.0

- **Linux version, new in this release.** `SPR-Setup-<version>-x86_64.AppImage` builds and runs the game on Linux and SteamOS. It is brand new and has not been played through from the package yet, so **please send your logs and open an issue for any problem**, even a small one, and also if it works: that is how the Linux version gets fixed. See *Reporting a bug* below for where the log is and how to get a more detailed one.
- **Linux shadows:** the flickering dark blocks in direct sunlight are gone and the frame interval went from 24 to 16.7 ms on the Linux path (thanks to azlekayn, PR #24).
- **AMD fixes, experimental.** None of them could be tested on an AMD GPU: they come from the logs AMD owners attached to issues #2 and #14 and from fixes other recompilations made for the same drivers. They may help, do nothing, or break something on AMD; NVIDIA and Intel take the same paths as before (except the recorder fence, which costs nothing there).
  - Draws into compressed (BC) textures that AMD's driver cannot render into directly now go through an intermediate image instead of being dropped (the logs showed `render target format 0x4b ... not renderable here: left out`). This may fix the grey grid in the island select menu and other missing effects.
  - When the driver has no mailbox present mode (AMD's), the game presents in relaxed FIFO: a frame that just misses the screen refresh is shown at once instead of waiting a whole refresh, which halved the frame rate near 60 fps (the ROG Ally running at 30 fps).
  - The Vulkan recorder thread makes its commands visible to the GPU before handing them over (an SFENCE), the cause of a device loss on AMD that mkw-recomp found. The recorder stays off by default on AMD.
- **Controllers:** when one controller shows up twice (a Flydigi Vader 5 Pro on its receiver), the one with motion sensors is used; rumble commands go out at most every 30 ms, which stopped those pads from freezing their input (thanks to azlekayn, PR #25 and #26).
- **Aiming up and down:** if the right stick only turns left and right, turn off **Motion Controls** in the game's own options (not the F1 menu): with it on, the game aims vertically with the gyro only (issues #2 and #23).
- **Setup:** when the install folder, the setup folder and your dump overlap, the setup now says which folder to move (issue #27).

## Downloads

| File | System | How to start |
|---|---|---|
| `SPR-Setup-<version>-windows.zip` | **Windows** 10 or 11, x86-64 | Unpack into an empty folder and run `SPR-Setup.exe`. |
| `SPR-Setup-<version>-x86_64.AppImage` | **Linux** and **SteamOS**, x86-64, glibc 2.34 or later | `chmod +x SPR-Setup-*.AppImage`, then `./SPR-Setup-*.AppImage`. When the setup is done, start the game from the install folder with `./spr-recomp --boot`. **Experimental**: new in 0.2.0. |

Both hold the same setup, which builds the game on your machine; neither holds game data.

**A release would hold no game data.** You build the game on your own PC from your own dump, given as a `.nup` file or as an ExeFS folder and a complete RomFS folder.

**Game versions: update 65536 is the only tested version.** The base game (1.0.0) and other updates are accepted, but their support is **experimental**: nothing says they build, start or play correctly, and the setup warns when it sees one. Please check a problem on update 65536 before reporting it.

## State

- The title, the opening cutscene, the character creator, the hub and the first gameplay zone render and play at about 60 fps on an RTX 3050, with the player's body drawn.
- The game's neural-network upscaler is stood in for by FSR 1 EASU.
- Resolution (F1 menu, Graphics, after restart): 1080p, 1440p or 4K docked; handheld renders 1280x720 at 1080p and 1920x1080 above. The higher modes are the game's own and have not been played through whole missions yet.
- The settings that act on Bananza patch sets (render scale, ultrawide, real-time frame timing) are hidden in the Raiders menu: the build has no patch sets.

## Requirements

- A GPU with Vulkan 1.3 and a recent driver; tested on NVIDIA only. **AMD support is experimental**: no AMD GPU has been tested by the maintainer, every AMD fix so far is experimental, and reports from AMD owners, working or not, are welcome.
- 16 GB of RAM plus a large page file (the process commits about 11 GB), 32 GB is comfortable.
- About 6 GB free for the build, besides the dump.

On Linux the build needs the same RAM and disk space, and a Vulkan 1.3 driver (Mesa RADV or NVIDIA's). Linux 6.7 or later is faster (write watch).

## Reporting a bug

Open an issue on https://github.com/Elie-0066/SPR-Recomp-Project/issues with the log, your GPU and driver, your system (Windows version, or Linux distribution and kernel), the SPR Recomp version and what you did before it failed. **Never share generated files, keys, builds or game files.**

The log is `logs/dkb.log` in the install folder, next to `spr-recomp.exe` (Windows) or `spr-recomp` (Linux); the four runs before it are `dkb.1.log` to `dkb.4.log`. The setup's own log is in its work folder, shown at the end of the setup window.

**A more detailed log.** For a slowdown, stutter or low frame rate, start the game with the performance trace on. It adds a line every 60 frames with the frame time, what held the frame back and the GPU time:

- Windows (Command Prompt, in the install folder): `set TOTK_TRACE=perf` then `spr-recomp.exe --boot`
- Windows (PowerShell, in the install folder): `$env:TOTK_TRACE="perf"; .\spr-recomp.exe --boot`
- Linux (terminal, in the install folder): `TOTK_TRACE=perf ./spr-recomp --boot`

AMD owners can help test the recorder fence: the recorder thread (which records the GPU commands on a second thread, faster) is off by default on AMD because it crashed at launch in 0.1.8. Start the game the same way with `TOTK_VK_RECORDER=1` instead of `TOTK_TRACE=perf`, and say in an issue whether it still crashes.

Play until the problem shows, close the game, then attach `logs/dkb.log`.
