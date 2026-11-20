# Installer

The installer builds SPR Recomp (*Splatoon Raiders*) on the player's machine from their own dump. The release carries
no game data and no code derived from the game. The installer comes from DKB Recomp, itself built on TotK Recomp's;
`installer/raiders.py` is this fork's profile, and it reuses the path checks, locks, translator step, host check and
step ids of DKB Recomp's `installer/bananza.py`.

## Input and versions

`installer/raiders.py`, selected with `--game raiders` (the window's default profile). It takes one `.nup` file, or an
ExeFS folder and a complete loose RomFS folder (for an update, the base RomFS with the update's laid over it, as dump
tools write it). No keys are requested, and NSP or XCI files are not read.

Versions. Update title version 65536 (`tools/data/expected-raiders.json`, `"verified": true`) is the version the project
is developed and tested on. Every other version is **experimental**: the check step emits
`warn.spr_version_experimental` and goes on, and nothing says the build starts or plays.

- The known versions are the records of `tools/data/expected-raiders*.json`, matched on all five ExeFS hashes:
  `expected-raiders.json` (update 65536) and `expected-raiders-base.json` (base 1.0.0, title version 0, never built).
- An ExeFS that matches none of them is accepted when it has the five files and is this game: a folder's `main.npdm`
  must carry program id `0400380020056000` (ACI0), a `.nup` package's title id must be that id or its update id
  (`spr.wrong_game` otherwise). Its version is named after the package's display version or title version.
- A `.nup` installs the newest update over the base by default; `--nup-package base` installs the base alone.
- Whatever the version, the compile step writes the identified record to `<temp>/expected-raiders.json` and passes it
  to `build_game.py --expected`, so `spr-recomp.exe` refuses any ExeFS but the one it was built from, and the settings
  menu shows its display version.

```
python -m installer --game raiders --nup "<dump>/raiders.nup" --dest "<install folder>" [--package "<tools>"] --jobs 8
python -m installer --game raiders --exefs "<dump>/updates/65536/exefs" --romfs "<dump>/updates/65536/romfs" --dest "<install folder>" --jobs 8
python -m installer --game raiders --nup "<dump>/raiders.nup" --nup-package base --dest "<install folder>"   # experimental
python -m installer.gui
```

### .nup input

A `.nup` is the container Eglantine and Eden load: loose ExeFS and RomFS trees for the base, its updates and add-on
content, every file raw or zstd-compressed and listed with its size and SHA-256 in a table of contents whose own
SHA-256 is in the superblock. `tools/extract_nup.py` reads it (an independent reader of Eglantine's
`core/file_sys/nup_container.cpp` format). What is installed follows Eglantine's loader (`core/loader/nup.cpp`,
`AppLoader_NUP`): the ExeFS of the newest update package (the base's when the update has none) and the base RomFS with
the update's laid over it. Preflight reads only the table of contents; when a selected file is zstd-compressed, the
Python that runs the tools must decompress zstd (`compression.zstd` of Python 3.14, the `zstandard` module or a `zstd`
program), or setup stops with `dkb.nup_zstd`.

## Setup steps

1. **Check.** Identify the version (above), list the RomFS, reject overlapping or linked input and output paths,
   validate the tool paths and the translator identity, then check disk space, AVX2 and Vulkan 1.3. Space: about 3 GB
   in the install folder plus the RomFS of a `.nup` (13 GB), and 6 GB of work space.
2. **Data.** Copy the ExeFS into `<dest>/exefs`. A RomFS folder is used where it is: the executable records its path,
   so it must stay there (setup lists its files, without hashing them). A `.nup` is extracted loose into `<dest>/romfs`
   with `tools/extract_nup.py --select latest` (or `base`; every file checked against its SHA-256 record), inside the
   install folder so the 13 GB tree is renamed into place, not copied.
3. **Translator.** Reuse `build/xlate/totk_xlate.exe` (or `--translator`) when its `.id` matches the translator
   sources, else build it with `tools/build_xlate.py` and the .NET 10 SDK (`--dotnet` selects a portable SDK).
4. **Lift.** `recomp/gen.py` lifts all three modules (`main`, `sdk`, `subsdk0`) without patch sets.
5. **Compile.** `tools/build_game.py --version raiders --full-build` links `spr-recomp.exe` against the recorded RomFS
   folder and writes `romfs-index.bin` (`tools/romfs_index.py`).
6. **Shaders.** `tools/shader_census.py --out <work> --seed <dest>/seed-nvn2.bin` lists the NVN 2 stages, then
   `spr-recomp.exe --translate-shaders` fills the shader cache and `--warm-shaders` builds the driver's pipeline
   libraries. If the latter crashes (seen on NVIDIA 621.01) setup warns and goes on.
7. **Verify.** Write the selected language into `dkb.ini` (the runtime's settings file) and run `--selftest`.
8. **Cleanup.** Remove the owned work folders. The generated C and the objects stay, so a newer runtime recompiles
   only the runtime. Setup does not start the game.

The install folder holds `spr-recomp.exe`, its runtime DLLs, `exefs/`, `romfs/` (for a `.nup`), `romfs-index.bin`,
`seed-nvn2.bin`, `dkb.ini`, `spr-install.json` and `shader-cache/`. Saves and further caches are created beside the
executable when it runs.

## Tools

A tool package (`--package`, the release layout below) supplies:

```
<tools>/llvm/bin/clang.exe, llvm-ar.exe, llvm-windres.exe
<tools>/runtime/deps/include/, lib/       Vulkan, VMA, SDL3, Opus, libvpx and zstd dependencies
<tools>/bin/ninja.exe, SDL3.dll, libopus-0.dll
```

In a checkout without one, setup builds with MSYS2 UCRT64 as [building.md](building.md) sets it up: its clang, ninja
and the SDL3, Opus and libiconv DLLs (`raiders.py` `msys2_layout`). `--toolchain`, `--deps`, `--ninja` and `--dll-dir`
override individual paths. No machine-specific path is built into the installer, and it never installs tools globally.

## Resume, ownership and cancellation

Only an empty destination or an SPR install with matching ownership metadata (`spr-install.json`) is accepted.
Temporary work has its own ownership marker; an unrelated nonempty temporary directory is refused. OS-held locks
prevent two setups from writing the same destination or workspace. Cancellation stops the installer's child process
tree, leaves completed steps recorded atomically, and never deletes source files, saves or settings.

Resume with the same command, or `python -m installer --dest "<install folder>"`. Each step records a key over its
inputs (`installer/runner.py`); a step with the recorded key and intact output is skipped, so resume and update are one
rule. Recorded source and tool paths are reused; keep the source folders available until setup completes. `--verify`
also hashes kept output. `--temp` selects a workspace base, kept apart from the destination and sources.

## Command line and window

- stdout: one JSON event per line, types `start`, `plan`, `step` (started, done, skipped), `progress` (step, phase,
  done/total/unit), `space`, `warning`, `error`, `end` (`installer/events.py`). Messages are ids with parameters;
  `installer/text/en.json` holds their text (Raiders' own ids start with `spr.`). The human-readable log goes to
  `<temp>/logs/`.
- Exit codes: 0 done, 1 failed, 2 bad command line, 3 cancelled. The window cancels with Ctrl+Break to the
  installer's process group.
- Window (`python -m installer.gui`, `installer/gui.py` draws, `installer/guilogic.py` decides): pickers for a `.nup`
  file or the ExeFS and RomFS folders, the optional tool package, language and destination; a step list with
  progress, a space line, a collapsible log, Cancel, and a finish screen with Open folder. English only.
  `--selftest [--shots DIR]` builds every screen and closes itself.
- Look (`installer/gui.py` THEMES): with `--game raiders` the window takes the in-game menu's Raiders colours
  (night blue, cyan ink, coral), an ink-splat drawing and `runtime/spr.ico`, which `tools/make_spr_icon.py` draws and
  `make_release.py` also links into `SPR-Setup.exe`; the other profiles keep the Donkey Kong look and `dkb.ico`.

## Release layout

`tools/make_release.py --game raiders` assembles the release folder and `--zip` packs it as
`SPR-Setup-<version>-windows.zip`; `.github/workflows/spr-release.yml` runs it, and publishes the zip on a `v*` tag
with the Linux AppImage ("Linux release" below).

```
SPR-Setup.exe         starts python/pythonw.exe -m installer.gui (installer/launcher/launcher.c, no console)
installer/            the installer: steps, CLI, window, English text catalog
python/               Python 3.14 embeddable + the installer's packages (+ tkinter for the window)
recomp/, tools/       the lifter, the build, the .nup reader, the shader census and the translator sources
llvm/                 llvm-mingw, x86_64-only subset (Apache-2.0 with LLVM exception; mingw-w64 parts permissive)
runtime/              every runtime source and header (the installer compiles the runtime) and deps/
bin/                  totk_xlate.exe, SDL3.dll, libopus-0.dll, ninja.exe
licenses/, THIRD_PARTY_NOTICES.md, dkb.ini.example, VERSION
```

- SDL3 comes from the upstream release, not MSYS2: MSYS2's DLL imports the LGPL libiconv.
- Every bundled third-party piece gets a row in `THIRD_PARTY_NOTICES.md`.
- The release contains nothing game-derived; `make_release.py` runs `tools/check_content.py` over the folder before it
  finishes, then the window's `--selftest` from the folder's own Python.

### Linux release

`tools/make_release_linux.py` (default `--game raiders`) assembles the same setup for Linux and SteamOS, and a bare
`--appimage` packs it as `SPR-Setup-<version>-x86_64.AppImage`. The `appimage` job of
`.github/workflows/spr-release.yml` runs it on Ubuntu 24.04 under `xvfb-run` and publishes the AppImage beside the
Windows zip. Everything that runs on the player's machine is built against the Steam Runtime 3 "sniper" SDK sysroot
(glibc 2.31), so the AppImage and the game it builds run on SteamOS and on any distribution at least as new.

```
AppRun, spr-setup.desktop, spr-setup.png   python/bin/python3 -m installer.gui --game raiders
installer/, recomp/, tools/                as in the Windows folder
python/               python-build-standalone 3.14 with an Xft Tcl/Tk, and the installer's packages
llvm/                 LLVM 23 clang and lld, and llvm/sysroot: the sniper files a game build reads
runtime/              every runtime source and header, totk.png (the window icon) and deps/ (Vulkan headers, VMA, zstd)
bin/                  totk_xlate, ninja, libSDL3.so.0 (the sysroot's; spr-recomp finds it beside itself)
licenses/, THIRD_PARTY_NOTICES.md, LICENSE, LICENSE-MIT, dkb.ini.example, VERSION
```

- The sysroot is cut down to the files a runtime compile and link read. The script learns them by compiling the runtime
  library once against a placeholder module layout (`placeholder_gen`), so it needs no game files and runs in CI.
- The AppImage is mounted read-only, so the setup works next to the install folder, in `<dest>/../.spr-setup`
  (`installer/raiders.py` main).
- It carries no `recomplauncher.json`: RecompLauncher sets up the Windows zip.
- The game needs a Vulkan 1.3 driver. Linux 6.7 or later gives it the write watch of `runtime/platform/linux.c`
  (asynchronous userfaultfd write protection); on an older kernel it reports every watched range as written, which
  costs speed. The first AppImage was built in CI and checked with the window's selftest only; it has not been
  played from the package yet. The runtime itself has been built from a checkout and played on Fedora 44 with an
  RX 7800 XT (RADV).
- There is no macOS package.

## Constraints

- The keys are never needed; no key value appears in the UI, logs or files.
- Paths may hold any Unicode characters. The Python steps write their files and logs as UTF-8, and `build_game.py`
  quotes paths with spaces on its Ninja command lines.
- Unsigned exe: SmartScreen warns on first start.
- Lightweight checks, with no game files: `python -m installer.gui --game raiders --selftest`.
