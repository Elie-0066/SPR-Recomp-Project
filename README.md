# Splatoon Raiders Recomp

This repository is a **fork of [DKB Recomp](https://github.com/Lucas-blip-png/dkb-pc-recomp)** retargeted from
*Donkey Kong Bananza* to *Splatoon Raiders* for Nintendo Switch 2. DKB Recomp is itself built on
[TotK Recomp](https://github.com/valentderah/totk-pc-recomp). The lifter, Horizon host, and Vulkan backend are largely
retained from those projects; this fork adds the Raiders target, its SDK instruction support, and the work needed for
its RomFS, NVN 2 calls, and shaders. Bananza documentation remains as reference material for shared parts of the
pipeline and is not a statement that Bananza is the active target.

The goal is a native Windows x86-64 executable. The ARM64 game code is translated to C ahead of time and compiled
with Clang. At run time, the host implements the Switch services and answers NVN 2 graphics calls through Vulkan.

## Status

**Early bring-up; not playable.** All three update modules (`main`, `sdk`, and `subsdk0`) now lift with no unsupported
instructions. The executable's ExeFS must match the dump used to generate its C: mixing the base lift with the update
binary caused a false jump into data. The update-aligned build now returns from `nninitStartup`, runs the module
initializers, enters `nnMain`, and mounts RomFS. It then hits a host access violation during startup, before a frame is
rendered. The update RomFS directory is indexed and read directly without packing a second copy of its contents.

| Phase | State |
| --- | --- |
| Dump intake | Extracted base and update ExeFS/RomFS are available locally. |
| Lifter | 259,913 functions in update 65536 across three modules; no unsupported instructions. |
| Horizon host | Update-aligned boot reaches `nnMain` and mounts RomFS; a host access violation during startup remains. |
| NVN 2 | DKB's host entry points are available; Raiders call coverage has not been surveyed. |
| Shaders | The census extracts 3,690 unique NVN 2 stages into a `DKBN2SD1` seed pack. The xlate translator now runs; Eglantine lowers 3,660 stages, with 30 remaining unsupported. Vulkan driver-cache preparation completed 4,940 variants without failures on this host. |
| First frame | Not reached. |

The detailed, current phase plan is [docs/raiders-plan.md](docs/raiders-plan.md). Shared architecture, build instructions,
and graphics notes are in [docs/architecture.md](docs/architecture.md), [docs/building.md](docs/building.md), and
[docs/maxwell.md](docs/maxwell.md). Bananza-specific notes remain in the `bananza-*` documents for reference.

## Game content boundary

This is a source-code fork; it does not include game files. Use your own legally obtained dump. Do not add game data,
keys, extracted assets, generated game code, or derived shader files to commits, issues, or pull requests. Local paths
belong in the ignored `config.local.toml`; generated and extracted files belong under ignored `build/` or `generated/`.
Instructions for obtaining game files are not provided.

## Build

The project uses the toolchain documented in [docs/building.md](docs/building.md): MSYS2 UCRT64 Clang and Ninja, plus
the repository's Python environments. With the Raiders update's ExeFS and RomFS directories already extracted:

```powershell
.venv\Scripts\python.exe recomp\gen.py --exefs <update-exefs> --out generated\raiders
.venv\Scripts\python.exe tools\build_game.py --version raiders --exefs <update-exefs> --romfs <update-romfs>
```

The build writes the executable and its derived index under `build\raiders`. Raiders' loose RomFS directory is indexed
in place, so it does not need to be repacked into a second multi-gigabyte image. Running the current bring-up build:

```powershell
build\raiders\splatoon-raiders.exe --boot
```

## Origins and licenses

This fork retains code from DKB Recomp and, through it, TotK Recomp. The relevant upstream attributions and terms are
in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). The repository as a whole is GPL-3.0-or-later because it includes
Eglantine's GPL shader lowering ([LICENSE](LICENSE)); files originating in TotK Recomp retain their MIT terms
([LICENSE-MIT](LICENSE-MIT)). Third-party code keeps its own license. The game and its trademarks are not licensed by
this repository. This is an independent, non-commercial fan project and is not affiliated with or endorsed by
Nintendo.
