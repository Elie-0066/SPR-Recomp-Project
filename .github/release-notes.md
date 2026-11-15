SPR Recomp: a static recompilation of *Splatoon Raiders* (Switch 2) to a native Windows exe with a Vulkan renderer. It is an experimental fork of DKB Recomp (itself built on TotK Recomp).

**A release would hold no game data.** You build the game on your own PC from your own dump, given as a `.nup` file or as an ExeFS folder and a complete RomFS folder.

**Game versions: update 65536 is the only tested version.** The base game (1.0.0) and other updates are accepted, but their support is **experimental**: nothing says they build, start or play correctly, and the setup warns when it sees one. Please check a problem on update 65536 before reporting it.

## State

- The title, the opening cutscene, the character creator, the hub and the first gameplay zone render and play at about 60 fps on an RTX 3050, with the player's body drawn.
- The game's neural-network upscaler is stood in for by FSR 1 EASU.
- Resolution (F1 menu, Graphics, after restart): 1080p, 1440p or 4K docked; handheld renders 1280x720 at 1080p and 1920x1080 above. The higher modes are the game's own and have not been played through whole missions yet.
- The settings that act on Bananza patch sets (render scale, ultrawide, real-time frame timing) are hidden in the Raiders menu: the build has no patch sets.

## Requirements (Windows)

- A GPU with Vulkan 1.3 and a recent driver; tested on NVIDIA only. **AMD support is unknown**: no AMD GPU has been tested yet, and reports from AMD owners, working or not, are welcome.
- 16 GB of RAM plus a large page file (the process commits about 11 GB), 32 GB is comfortable.
- About 6 GB free for the build, besides the dump.

## Reporting a bug

Open an issue on https://github.com/Elie-0066/SPR-Recomp-Project/issues with the log (`logs\dkb.log` next to `spr-recomp.exe`), the commit you built, your GPU and driver, and what you did before it failed. **Never share generated files, keys, builds or screenshots that show game content.**
