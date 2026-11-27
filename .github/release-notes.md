## v0.2.3: a fix for the AMD crash during combat, a loading screen at boot and faster draws

# **Don't forget to follow me on X for news about the next releases: https://x.com/Lyriiiie**


SPR Recomp: a static recompilation of *Splatoon Raiders* (Switch 2) to a native Windows and Linux executable with a Vulkan renderer. It is an experimental fork of DKB Recomp (itself built on TotK Recomp).

## New in 0.2.3

Each entry says whether it is **tested** or **experimental**. Experimental means that the code is in, but the maintainer has no hardware or setup to confirm it: please report whether it works for you.

- **AMD: the crash during combat is fixed** (issue #39) - **tested** on Linux by two players, on an RX 9060 XT and an RX 7800 XT (Mesa RADV): the mission that crashed every time (the treasure mission with Big Man, in the second stage) now plays through. Some shaders asked a texture for a size it does not have, and the translator wrote that as invalid shader code. NVIDIA ignored it; AMD's Linux driver crashed while it built the shader. The fix is in the shader code itself, so it applies on Windows and Linux and on every GPU. Whether AMD's Windows driver also crashed on it is not known. The fix is in two places: the translator no longer writes that code (after NX2navi, thanks to weluvjr, DieMitchell and tangerineturtle1), and the game also repairs shaders already in your shader cache (thanks to azlekayn). It does not fix the AMD crash in level 3 (issue #28), which is a different problem.
- **A loading screen at boot** - **tested** on Linux with an AMD GPU and on Windows with an RTX 3050. The first launch after an update translates the shaders and builds the pipelines before the game starts; the window used to stay black meanwhile, so it looked hung. It now shows the progress with an estimate of the time left. Thanks to azlekayn.
- **Draws find their pipeline faster** - **tested** on Linux with an AMD GPU and on Windows with an RTX 3050: a cache of the recent pipeline states cuts the CPU time spent per draw. Thanks to azlekayn.
- **Linux: a crash inside the GPU driver is logged** with the shader the driver was building, and that shader is written to the `logs` folder as a `.spv` file you can attach to an issue (it holds no game images). On Windows, a crash inside the driver is not captured yet. Thanks to azlekayn.

**The first launch after the update translates the shaders again.** It takes a few minutes, once; the new loading screen shows it.

**Not fixed yet:** the AMD crash in level 3 (issue #28) and the horizontal streaks and washed-out sky and water on AMD (issues #14 and #29). The causes are not known. If you have an AMD GPU, a frame dump helps most: start the game with `TOTK_DUMP=key` set the same way as the variables in *Reporting a bug*, press F9 while the glitch is on screen, and attach `passes.txt` from the `dump` folder. **Do not attach the PNG files or any other dump file**: they can hold game images.

Everything from 0.2.2 (the gyro through Steam, the 5120x2160 ultrawide shape, the shader fixes from DKB Recomp) is in this release too.

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
- About 6 GB free for the build, besides the dump, plus 13 GB for the RomFS when the install folder is on another disk than the dump.

On Linux the build needs the same RAM and disk space, and a Vulkan 1.3 driver (Mesa RADV or NVIDIA's). Linux 6.7 or later is faster (write watch).

## Reporting a bug

Open an issue on https://github.com/Elie-0066/SPR-Recomp-Project/issues with the log, your GPU and driver, your system (Windows version, or Linux distribution and kernel), the SPR Recomp version and what you did before it failed. **Never share generated files, keys, builds or game files.**

The log is `logs/dkb.log` in the install folder, next to `spr-recomp.exe` (Windows) or `spr-recomp` (Linux); the four runs before it are `dkb.1.log` to `dkb.4.log`. The setup's own log is in its work folder, shown at the end of the setup window.

**A more detailed log.** For a slowdown, stutter or low frame rate, start the game with the performance trace on. It adds a line every 60 frames with the frame time, what held the frame back and the GPU time:

- Windows (Command Prompt, in the install folder): `set TOTK_TRACE=perf` then `spr-recomp.exe --boot`
- Windows (PowerShell, in the install folder): `$env:TOTK_TRACE="perf"; .\spr-recomp.exe --boot`
- Linux (terminal, in the install folder): `TOTK_TRACE=perf ./spr-recomp --boot`

**AMD: a log for a crash or a graphics glitch.** Start the game with the GPU breadcrumbs, which name in the log the last shaders the GPU ran before a crash, together with the performance trace:

- Windows (Command Prompt, in the install folder): `set TOTK_CRUMB_BIND=1` then `set TOTK_TRACE=perf` then `spr-recomp.exe --boot`
- Windows (PowerShell, in the install folder): `$env:TOTK_CRUMB_BIND="1"; $env:TOTK_TRACE="perf"; .\spr-recomp.exe --boot`
- Linux (terminal, in the install folder): `TOTK_CRUMB_BIND=1 TOTK_TRACE=perf ./spr-recomp --boot`

The game runs a little slower this way. Play until the crash or the glitch, then attach `logs/dkb.log` (and `dkb.1.log` if you started it again since) with a screenshot of the glitch, your GPU and driver version.

AMD owners can help test the recorder fence: the recorder thread (which records the GPU commands on a second thread, faster) is off by default on AMD because it crashed at launch in 0.1.8. Start the game the same way with `TOTK_VK_RECORDER=1` instead of `TOTK_TRACE=perf`, and say in an issue whether it still crashes.

Play until the problem shows, close the game, then attach `logs/dkb.log`.
