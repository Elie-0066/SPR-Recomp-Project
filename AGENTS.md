# SPR Recomp

> **This is a fork of DKB Recomp retargeted from Donkey Kong Bananza to Splatoon Raiders (Switch 2).** The lifter,
> Horizon host and Vulkan back end are largely retained from DKB Recomp and its upstream base, TotK Recomp. Read
> [docs/raiders-plan.md](docs/raiders-plan.md) first; Bananza documents remain reference material for shared pipeline
> behaviour, not the active game target.

Static recompilation of *Splatoon Raiders* (Switch 2, ARM64) into a native Windows x86-64 executable with a Vulkan renderer. This fork is based on [DKB Recomp](https://github.com/Lucas-blip-png/dkb-pc-recomp), itself based on [TotK Recomp](https://github.com/valentderah/totk-pc-recomp). The active target plan is [docs/raiders-plan.md](docs/raiders-plan.md); shared architecture, build, and graphics references are [docs/architecture.md](docs/architecture.md), [docs/building.md](docs/building.md), and [docs/maxwell.md](docs/maxwell.md). Bananza-specific documents remain historical references.

## Game content boundary

The repository is private for now, and is written so that it could be made public: game data lives outside it and every derived file stays in ignored folders.

- The user's game and key paths are in `config.local.toml` (ignored). Pass the keys file path to tools; the tools read it themselves. Key values stay out of your output, logs, commands and files.
- Everything extracted or generated goes under `build/` or `generated/`.
- Every file the code generator emits carries the marker defined as `GENERATED_MARKER` in `tools/check_content.py`.
- Run `python tools/check_content.py --staged` before proposing a commit. It is the same check CI runs.

## Reusing code

`refs/` holds the upstream checkouts (`tools/fetch_refs.sh` recreates it). Read-only.

- Copy only MIT, BSD, ISC, zlib or Apache-2.0 code, keep its header, and add a row to `THIRD_PARTY_NOTICES.md`.
- The repository is GPL-3.0-or-later as a whole (LICENSE), as DKB Recomp is, because it carries Eglantine's GPL Ampere lowering
  (`runtime/third_party/eglantine/`, with the owner's go-ahead). GPL-3.0-compatible code (Eden, Eglantine, yuzu and
  the like) may be copied with its header kept, a row in `THIRD_PARTY_NOTICES.md` and the upstream commit. Files that
  came from TotK Recomp keep their MIT terms (`LICENSE-MIT`); a fix meant for TotK Recomp itself must not use GPL code.
- `tools/vendor/` stays byte-identical to upstream; Windows changes go in `tools/extract.py`.

## Code

- Build C with MSYS2 UCRT64 Clang (`C:\msys64\ucrt64\bin`). Generated C uses GNU extensions, so `cl.exe` is out. Call Ninja from that directory by full path or put it first on PATH.
- Two Python environments: `.venv` (3.12) for the lifter and the build, `.venv-py314` for the extraction and shader scripts, which need `compression.zstd`.
- Generated C is never edited by hand: change the generator (`recomp/`).
- Every lifter change passes the differential tests against Unicorn ([docs/building.md](docs/building.md), "Checks"). After changing `recomp/jumptables.py`, `tests\jumptables_check.py` reports 0 disagreements on 1.2.1 and 1.4.3.
- `recomp/layout.py` alone decides module bases.
- Graphics bugs: [docs/maxwell.md](docs/maxwell.md) lists the Maxwell behaviour the Vulkan path must match and the TotK bugs other ports fixed.
- Raiders bugs (NVN 2, the GPU, Switch 2 services such as `nn::codec`): first read how `refs/eglantine` handles the same point. That emulator runs Switch 2 games, so it is the default reference. Where it departs from the hardware on purpose (Eden's default GPU accuracy skips texture downloads on CPU reads, `RasterizerVulkan::MustFlushRegion`), say so instead of copying the shortcut.
- Comments say what the code does and why, in words that stand on their own. Cite our code by file plus function (a line number may follow as a hint) and upstream code by project, file and function: `refs/` moves, so its line numbers rot. Commit hashes belong to the attribution of copied code.

## Writing

Code, comments, docs and commit messages are in English. Commit messages are plain sentences without type prefixes.
