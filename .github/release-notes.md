## v0.2.1: Linux and SteamOS setup fixes, a self-contained install folder and ultrawide window sizes

# **Don't forget to follow me on X for news about the next releases: https://x.com/Lyriiiie**


SPR Recomp: a static recompilation of *Splatoon Raiders* (Switch 2) to a native Windows and Linux executable with a Vulkan renderer. It is an experimental fork of DKB Recomp (itself built on TotK Recomp).

## New in 0.2.1

- **Linux and SteamOS setup fixed.** The AppImage of 0.2.0 stopped on SteamOS at the computer check (`stdio.h` not found, issue #30) and on other distributions in the compile (`SDL3/SDL.h` not found, issue #31): the C headers it carries had left out SDL3, Opus and libvpx, and the check did not use them at all. Both now build from the AppImage alone. Please keep sending Linux logs, working or not.
- **The install folder holds everything** (issue #32). When you give the setup an ExeFS and a RomFS folder, the RomFS now goes into `<install folder>/romfs` as well, so the game keeps running after you move or delete your dump. On the same disk the files are hard links and take no extra space; on another disk they are copied (about 13 GB). An update rebuilds from that copy when the dump is gone.
- **Ultrawide window sizes** (issue #13). The *Window size* row of the F1 menu now lists the 21:9 and 32:9 sizes that fit your monitor (2560x1080, 3440x1440, 3840x1600, 5120x2160, 3840x1080, 5120x1440) and your monitor's own size. The game's own wide picture is the *Ultrawide* row of the Graphics group (up to 3840x1600; the game cannot render wider than 3840).
- **AMD: a detailed crash log.** The AMD crash in level 3 (issue #28) is a GPU fault the normal log cannot place. If the game crashes on your AMD GPU, see *Reporting a bug* below for how to start it with GPU breadcrumbs and attach that log.

Everything from 0.2.0 (the AMD fixes, the controller fixes, the Linux shadows) is in this release too.

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
