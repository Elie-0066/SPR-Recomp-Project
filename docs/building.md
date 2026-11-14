# Building from source

How to build SPR Recomp, the recompilation of *Splatoon Raiders*, from your own dump. Windows 10 or later, an x86-64
CPU with AVX2, and a GPU with Vulkan 1.3 and `VK_EXT_external_memory_host`. Besides the dump, allow about 6 GB for the
build (1.3 GB of generated C, 0.4 GB of objects, the 300 MB executable and the shader and pipeline caches).

Players who only want to play can use the [installer](installer.md) instead: it builds the game on their machine
without this toolchain.

## Toolchain

Versions are the ones the build was verified with; newer patch releases are fine.

| Tool | Version | Install | Used for |
|---|---|---|---|
| MSYS2 UCRT64 | clang/lld 23.1.3 | msys2.org installer to `C:\msys64`, packages below | compiling the generated C and the runtime |
| Visual Studio Build Tools | 2026, C++ workload, Windows SDK 10.0.26100 | winget `Microsoft.VisualStudio.BuildTools` with `--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended` | the linker .NET NativeAOT calls |
| .NET SDK | 10.0 | winget `Microsoft.DotNet.SDK.10` | the shader translator (`totk_xlate.exe`) |
| Vulkan SDK | 1.4.363 | winget `KhronosGroup.VulkanSDK` | validation layers, `vulkaninfo` |
| uv | 0.13 | standalone installer from astral.sh | the two Python environments |

MSYS2 packages, after `pacman -Syuu` (run it twice; the first pass updates the core and exits):

```sh
pacman -S --needed --noconfirm mingw-w64-ucrt-x86_64-{clang,lld,ninja,gcc,make,pkgconf,sdl3,opus,libvpx,zstd,vulkan-devel,\
vulkan-memory-allocator,vulkan-validation-layers,shaderc,spirv-tools} base-devel git unzip
```

`ffmpeg` from MSYS2 is a GPLv3 build and is never linked; the movies go through libvpx.

PATH:

- Add `C:\Program Files (x86)\Microsoft Visual Studio\Installer` to the user PATH: NativeAOT needs `vswhere.exe`, or
  the publish fails at the link step (MSB3073).
- Another `cmake` or `ninja` earlier on PATH (a 32-bit winlibs MinGW, for example) is picked up by accident. The build
  scripts call Ninja from `C:\msys64\ucrt64\bin` themselves; from a shell, put `C:\msys64\ucrt64\bin` first.

## Build (Splatoon Raiders)

This is the active target: update `65536` of *Splatoon Raiders* (title `0400380020056000`), read from your own
extracted dump. From the repository root, in PowerShell:

1. Python environments (once). `.venv` runs the lifter and the build; `.venv-py314` runs the extraction and shader
   scripts, which need the stdlib `compression.zstd` of Python 3.14:

   ```powershell
   uv venv --python 3.12 .venv
   uv pip install --python .venv/Scripts/python.exe -r tools/requirements.txt
   uv venv --python 3.14 .venv-py314
   uv pip install --python .venv-py314/Scripts/python.exe -r tools/requirements-py314.txt
   ```

2. Copy `config.example.toml` to `config.local.toml` and set the paths of your dump there. `config.local.toml` is
   ignored by git. Raiders reads the extracted update ExeFS and RomFS folders in place: no keys, no repacking.

3. Shader translator (once; no game files). This publishes `build\xlate\totk_xlate.exe`:

   ```powershell
   .venv\Scripts\python tools\build_xlate.py
   ```

4. Lift the three ExeFS modules (`main`, `sdk`, `subsdk0`) to C, then compile and link `build\raiders\spr-recomp.exe`.
   The build writes the ExeFS hash beside the executable, so it refuses to start with a different dump:

   ```powershell
   .venv\Scripts\python recomp\gen.py --exefs "<dump>\updates\65536\exefs" --out generated\raiders
   .venv\Scripts\python tools\build_game.py --version raiders --exefs "<dump>\updates\65536\exefs" --romfs "<dump>\updates\65536\romfs"
   ```

   The generated C is about 1.1 GB. A later build recompiles only what changed; after a runtime change,
   `ninja -f build\raiders\build.ninja` from the repository root relinks. A `game.ico` in `build\raiders` becomes the
   executable's icon.

5. Prepare the shaders (once per build folder and translator). The census collects every NVN 2 stage of the game's
   files into `seed-nvn2.bin`, then the executable translates them and builds the driver's pipeline libraries:

   ```powershell
   .venv-py314\Scripts\python tools\shader_census.py --version raiders --romfs "<dump>\updates\65536\romfs" --exefs "<dump>\updates\65536\exefs" --out build\raiders\shaders --seed build\raiders\seed-nvn2.bin
   cd build\raiders
   .\spr-recomp.exe --translate-shaders
   .\spr-recomp.exe --warm-shaders
   ```

   Without this step the first launch compiles shaders during play and stutters until the caches fill.

6. Run, from the build folder (the runtime looks for its files there):

   ```powershell
   cd build\raiders
   .\spr-recomp.exe --boot
   ```

Settings live in `dkb.ini` next to the executable (the in-game menu writes it); saves go to `build\profile-raiders`.
The current state of the game is in [raiders-plan.md](raiders-plan.md).

## Other systems

The runtime has Linux and macOS ports inherited from TotK Recomp and DKB Recomp (`runtime/platform/linux.c`,
`runtime/platform/darwin.c`, the SDL3 window and input files `*_sdl.c`). SPR Recomp has not been built or run on
either. Their build steps and their known limits are documented in the upstream projects.

## Checks

Before a commit:

- `python tools/check_content.py --staged`: nothing game-derived is staged. CI runs the same check;
  `git config core.hooksPath .githooks` runs it before each local commit.
- `build\raiders\spr-recomp.exe --selftest`: layout, dispatch table and host-function registry, without game files.

The lifter (`recomp/`) and the CPU headers (`runtime/cpu.h`, `runtime/simd.h`) are checked against Unicorn. Every lifter
change passes these, all run with `.venv`:

| Test | Checks | Game files |
|---|---|---|
| `tests\diff\lift.py` | instructions sampled from the extracted modules; `--coverage` lists every word the lifter rejects or traps | yes |
| `tests\diff\simd.py` | the whole FP/SIMD encoding space | no |
| `tests\diff\atomic.py` | exclusive load/store sequences and ARMv8.1 atomics | no |
| `tests\diff\func.py` | whole functions as the generator emits them, loops, jump tables and calls included (calls go to a stub that checks the registers at the call) | yes |
| `tests\flags_check.py` | the function-local flag helpers of `runtime\cpu.h` against its packed ones | no |
| `tests\excl_mt.py` | the exclusive monitor helpers under contention and ABA, on several threads | no |
| `tests\codec_abi_test.py`, `tests\jcms_abi_test.py`, `tests\system_services_test.py`, `tests\nvn_version_test.py` | the `nn::codec` VP9 ABI, the Jcms peripheral stubs, the system service host bindings and the NVN minor version per target | no |
| `tests\input_script_test.py --cc <clang>` | the scripted input parser and scheduler (`runtime/input_script.c`) on synthetic clocks: ticks, timed events, holds, mixed scripts, the landing summary | no |
| `tests\fg_pace_test.py --cc <clang>` | the pacing arithmetic of the frame generation (`runtime/fg_pace.h`) on a synthetic clock: the interval average, its clamps, the hitch rule, even spacing | no |
| `tests\aoc_romfs_test.py --cc <clang>` | the add-on content reader on synthetic TOTKRFS1 packs: bad and unsorted indexes, EOF reads, handle table, unmount and remount, `ListAddOnContent` paging | no |
| `tests\lowend_bench_test.py` | `tools\lowend_bench.py` `parse_log` on a synthetic perf log (the tool itself runs `dkb.exe` pinned to N cores and prints a before/after table) | no |
| `tests\jumptables_check.py` | `recomp/jumptables.py` against a Unicorn oracle; must report 0 disagreements, on 1.4.3 too | yes |

Runtime probes, each a mode of the executable (`spr-recomp.exe`) (the list is at the top of `runtime/main.c`): `--probe-kernel`,
`--probe-vkexec`, `--probe-nvn-tex` and `--probe-shader` cover most runtime changes. `--probe-shader` and the draw
part of `--probe-vkexec` translate shaders of the census below: they look for `build/<ver>/shaders/jobs.json` of the
exe's own version first, then of the other one. On macOS a probe that opens a window needs a desktop session, because
the SDL video driver initialises on the process's main thread.

Shaders: `.venv-py314\Scripts\python tools\shader_census.py --version <ver> --translate` extracts every Maxwell shader
to `build\<ver>\shaders\` and runs it through the translator and `spirv-val`; failure groups go to `failures.txt`.
`tools\xlate_smoke.py` checks the translator alone, without game files.

## Reverse engineering (optional)

- Ghidra 12.1.2 with Adubbz's Ghidra-Switch-Loader in `toolchain\ghidra\` (JDK 21) opens the game modules (NSO).
- `tools\fetch_refs.sh` clones the upstream projects this port builds on into `refs\`, read-only.
