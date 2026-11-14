# SPR Recomp

An unofficial static recompilation of *Splatoon Raiders* (Nintendo Switch 2) into a native Windows x86-64 executable
with a Vulkan renderer. It is a **fork of [DKB Recomp](https://github.com/Lucas-blip-png/dkb-pc-recomp)**, retargeted
from *Donkey Kong Bananza*; DKB Recomp is itself built on [TotK Recomp](https://github.com/valentderah/totk-pc-recomp).
The lifter, Horizon host, and Vulkan backend are largely retained from those projects; this fork adds the Raiders
target, its SDK instruction support, and the work needed for its RomFS, NVN 2 calls, and shaders. Bananza
documentation remains as reference material for shared parts of the pipeline.

The ARM64 game code is translated to C ahead of time and compiled with Clang. At run time, the host implements the
Switch services and answers NVN 2 graphics calls through Vulkan.

**This project is not affiliated with or endorsed by Nintendo. It contains no game code or data: you need your own
copy of the game, and the executable you build from it is for your own use only. Do not share builds.**

(This is also HEAVILY Vibecoded and experimental just so you know, if there is any problem let me know)

## Status

**Early, playable in parts.** The title, the opening cutscene, the character creator, the hub and the first gameplay
zone render and play at about 60 fps on the test machine (Ryzen 5 4500, RTX 3050). Known open issue: the game's
neural-network upscaler is stood in for by FSR 1 EASU.

**Game versions.** Update 65536 is the only version SPR Recomp is developed and tested on. The setup also builds the base
game (1.0.0) and later updates, but that support is **experimental**: it is not certain that they build, start or play
correctly. The setup says so with a warning when it sees another version. Report problems on another version only after
checking them on update 65536.

| Phase | State |
| --- | --- |
| Dump intake | Extracted base and update ExeFS/RomFS are read in place; no keys needed. |
| Lifter | 259,913 functions in update 65536 across three modules; no unsupported instructions. |
| Horizon host | Boots through `nnMain`, the RomFS mount, HID, audio, saves and the character creator. |
| NVN 2 | Draws, passes, compute and presents work; a few procs (DLSS, dirty tile maps) are still missing. |
| Shaders | Eglantine lowers all 3,690 seed stages. |
| Gameplay | The first zone plays at about 60 fps, with the player's body. |

The detailed, current phase plan is [docs/raiders-plan.md](docs/raiders-plan.md). Shared architecture, build instructions,
and graphics notes are in [docs/architecture.md](docs/architecture.md), [docs/building.md](docs/building.md), and
[docs/maxwell.md](docs/maxwell.md). Bananza-specific notes remain in the `bananza-*` documents for reference.

## Game content boundary

This is a source-code fork; it does not include game files. Use your own legally obtained dump. Do not add game data,
keys, extracted assets, generated game code, or derived shader files to commits, issues, or pull requests. Local paths
belong in the ignored `config.local.toml`; generated and extracted files belong under ignored `build/` or `generated/`.
Instructions for obtaining game files are not provided.

## Requirements

- Windows 10 or 11 on an x86-64 CPU with AVX2. Linux and macOS are not supported.
- A GPU with Vulkan 1.3 and a current driver (tested on NVIDIA only).
- 16 GB of RAM plus a large page file (the process commits about 11 GB); 32 GB is comfortable.
- About 6 GB free for the build, besides the dump.
- Your own dump of *Splatoon Raiders* update 65536, as a `.nup` file or as extracted ExeFS and RomFS folders. No keys are
  needed.
- To build from source: the toolchain in [docs/building.md](docs/building.md) (MSYS2 UCRT64 Clang, Ninja, Visual
  Studio Build Tools, Python 3.12 and 3.14, the .NET SDK for the shader translator).

## Install

The setup builds the game in one go from a `.nup` file or the extracted update folders, identifies the dump's version first
(warning when it is not the tested update 65536), and leaves a ready folder with `spr-recomp.exe` ([docs/installer.md](docs/installer.md)):

```powershell
.venv\Scripts\python.exe -m installer --game raiders --exefs <update-exefs> --romfs <complete-romfs> --dest <install-folder>
.venv\Scripts\python.exe -m installer.gui
```

## Build

The project uses the toolchain documented in [docs/building.md](docs/building.md): MSYS2 UCRT64 Clang and Ninja, plus
the repository's Python environments. With the Raiders update's ExeFS and RomFS directories already extracted:

```powershell
.venv\Scripts\python.exe recomp\gen.py --exefs <update-exefs> --out generated\raiders
.venv\Scripts\python.exe tools\build_game.py --version raiders --exefs <update-exefs> --romfs <update-romfs>
```

The build writes the executable and its derived index under `build\raiders`. Raiders' loose RomFS directory is indexed
in place, so it does not need to be repacked into a second multi-gigabyte image. Running it:

```powershell
build\raiders\spr-recomp.exe --boot
```

Running, settings and logs are covered in [docs/dkb-testing.md](docs/dkb-testing.md); controls in
[docs/controls.md](docs/controls.md).

## Reporting problems and contributing

Bugs go to the [issue tracker](https://github.com/Elie-0066/SPR-Recomp-Project/issues), with the template that fits: the commit you built, your GPU, driver and
Windows version, the log (`logs\dkb.log` next to `spr-recomp.exe`) and the steps to reproduce. Never attach game files,
keys, builds, shader caches or screenshots of the game. Issues about Raiders do not belong on the DKB Recomp or TotK
Recomp trackers.

Pull requests are welcome; read [CONTRIBUTING.md](CONTRIBUTING.md) first, and enable the content check before your first
commit:

```sh
git config core.hooksPath .githooks
```

Everyone taking part follows the [Code of Conduct](CODE_OF_CONDUCT.md).

## Credits

SPR Recomp stands on the work of [DKB Recomp](https://github.com/Lucas-blip-png/dkb-pc-recomp) and
[TotK Recomp](https://github.com/valentderah/totk-pc-recomp) and their contributors, whose commits and authors are kept
in this repository's history, and on the projects listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md), among them
LAS Recomp, MacNativeGames, Eglantine (Eden), Ryujinx, Dear ImGui and SDL.

## Origins and licenses

This fork retains code from DKB Recomp and, through it, TotK Recomp. The relevant upstream attributions and terms are
in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). The repository as a whole is GPL-3.0-or-later because it includes
Eglantine's GPL shader lowering ([LICENSE](LICENSE)); files originating in TotK Recomp retain their MIT terms
([LICENSE-MIT](LICENSE-MIT)). Third-party code keeps its own license. The game and its trademarks are not licensed by
this repository. This is an independent, non-commercial fan project and is not affiliated with or endorsed by
Nintendo.
