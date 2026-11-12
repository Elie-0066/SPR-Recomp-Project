# Splatoon Raiders Recomp — plan

This repository is a fork of [DKB Recomp](https://github.com/Lucas-blip-png/dkb-pc-recomp), retargeted from *Donkey
Kong Bananza* to *Splatoon Raiders* (Nintendo Switch 2). DKB Recomp is itself built on
[TotK Recomp](https://github.com/valentderah/totk-pc-recomp). No emulator runs at play time: the game's ARM64 code
is lifted to C ahead of time, compiled with Clang, and its NVN 2 graphics calls are answered by a host implementation
that renders through Vulkan.

The lifter, Horizon host, and Vulkan backend are largely retained from DKB Recomp and its upstream projects. No game
files are in this repository and none will be. The build reads the user's own dump from
`config.local.toml`; everything derived from it stays under `build/` and `generated/` (both git-ignored).

## Dump

| Item | Value |
|---|---|
| Dump root | `C:\Trucs jsp\Crack\Splatoon Raiders\SW2-dumpster` |
| ExeFS | `base\exefs` (`main`, `sdk`, `subsdk0`, `rtld`, `main.npdm`) |
| RomFS | `base\romfs`, 46,339 files, 12.7 GiB |
| Update | `updates\65536` (complete ExeFS, incremental RomFS overlay) |
| Program id | `0x0400380020056000` (ACI0 at `main.npdm`+0x560) |
| Module name | `Spiral` (in `main`, at the end of the NSO header) |
| Executable | NSO0, flags `0x7F`: text/rodata/data LZ4-compressed, hashes valid, execute-only, no ZBIC |
| Imports | `main`: 1,270 (needs `movie.nss`, `nnSdk.nss`); `subsdk0`: 257; `sdk`: 8 |

The base and the update share the same layout; the update's `main` is 4,801 bytes larger and its text is 0x16C0 bytes
larger. The first target is the update (`v65536`), because it is the version users have installed; base 1.0.0 stays as
a fallback if a regression has to be bisected.

## Engine notes

- Nintendo EPD's engine of the Splatoon family: RomFS top level `Phive`, `Gyml`, `AS`, `Banc`, `Mals`, `Logic`,
  `RSDB`, `SLink2`, `ELink2`, plus `Shader`, `Sound`, `UI`, `Model`, `Tex`, `Effect`, `Pack`.
- Graphics is NVN 2 (`Nin_Ounce_NVN2` in asset names). `main` imports only `nvnBootstrapLoader` and `nvnPrivate0`:
  the function table is fetched at run time, exactly as in DKB Recomp, so the whole NVN 2 surface is intercepted at
  one place.
- Shaders are precompiled in the RomFS: the update contains `Shader\Hoian_UBER.Product.110.product.Nin_Ounce_NVN2.bfsha.zs`
  and `Shader\Sample.Nin_Ounce_NVN2.release.sarc.zs`. The census reads the loose base RomFS plus the update overlay,
  decompresses zstd frames, walks SARC members and extracts short-control NVN 2 stages embedded in FSHA/BNSH.
  It found 3,690 unique stages and wrote the ignored `build\raiders\shaders\seed-nvn2.bin` pack, also copied to
  `build\raiders\seed-nvn2.bin` where the runtime expects it. The xlate translator runs through a local .NET-host
  launcher. Runtime lowering accepts 3,660 stages; 30 still fail Eglantine lowering (29 byte-to-float modes and one
  multiply-wide addend mode). Driver-cache preparation completed 4,940 of 4,940 Vulkan stage variants with no failures.
- `main` imports `movie.nss`; the sdk module exports the `nn::movie` classes. The host needs the codec surface
  DKB implemented for VP9 (`runtime/ov_codec.c`, libvpx).

## Host surface

Measured from the ExeFS export/import tables (the loader resolves imports by name, so this is the whole host contract):

- `main` imports 1,270 symbols. 1,256 are exports of the lifted `sdk` (`nnSdk.nss`) and `subsdk0` (`movie.nss`), so
  they stay guest code: `nn::os`, `nn::fs`, `nn::oe`, `nn::am`, `nn::codec`, the C++ ABI and libc all arrive as
  lifted ARM64. The remaining 14 are **weak**, so the loader binds them to 0 as the console does when the optional
  library is absent: zstd's tracing hooks from `nn::pia` (`ZSTD_trace_{compress,decompress}_{begin,end}`), two gRPC
  thread-local guards (`_ZTHN9grpc_core...`) and Nintendo's runtime shader compiler (`glslcCompile*`, `glslcInitialize`,
  `glslcSetAllocator`, `glslcGetVersion`, `glslcFinalize`, ...). They need no host implementation, only a log line if
  one is ever called.
- `sdk` imports 8 symbols: `malloc`, `free`, `calloc`, `realloc`, `aligned_alloc` and
  `__nnDetailNintendoSdkRuntimeObjectFile`, all defined by `main` (its own musl), plus `nnMain` and `nninitStartup`,
  which `main` exports for `sdk` to call back into. `subsdk0` imports 257, all defined by `sdk` and `main`.
- `main.npdm` asks for priority 44, core 0 and a 1 MiB stack, the values `runtime/boot.c` `check_npdm` already
  requires. `main` exports nothing named `nn::init::Start`; `sdk` does (`_ZN2nn4init5StartEmmPFvvES2_S2_`), which is
  the entry point `runtime/boot.c` calls.

So the first boot needs no new host function at all: what the port must supply is the SVC layer (`runtime/svc.c`,
`kernel.c`, `memory.c`) and whatever the guest `nnSdk` asks the kernel for.

## Status

| Phase | | State |
|---|---|---|
| P0 Dump intake | ✅ | Loose ExeFS/RomFS read in place; no keys needed. NSO0 header parsed; module name and build ids read. |
| P1 Lifter | ✅ | Update 65536 lifted with **0 unsupported instructions**: `main` 212,406, `sdk` 45,972, `subsdk0` 1,535. 1,107 MB + 151 MB + 9 MB of C. Generated modules must match the ExeFS loaded by the runtime; segment boundaries differ between base and update. |
| P2 Horizon host | ✅ | Boots through `nnMain`, the RomFS mount, HID, audio, saves and the character creator. `nn::nfp` (amiibo) and `nn::ngc::ProfanityFilter` (name check) are answered by `runtime/ov_system.c`; without them the game opens services the host lacks (`nfp:user`, `ngc:u`) and `sm` is fatal. |
| P3 NVN 2 front end | partial | Draws, passes, compute and presents work. Missing procs are still listed in the log (DLSS, dirty tile map, cooperative vectors). |
| P4 Shaders | partial | Eglantine lowers all but 9 stages: 8 large compute variants of the upscaler (`CALL` linkage) and one vertex stage (a conditional `IMAD` with a constant-buffer addend whose bits 62-63 are set). Lowering gained signed `I2F.S8/S16`, `TXQ` mask 4 and `UIMAD.WIDE` with an immediate. |
| P5 First frame | ✅ | Title, difficulty choice, the opening cutscene and the character creator render at about 55 fps. |


### Bring-up findings

Each one cost a session; they are listed so the next one does not repeat the search.

- **The upscaler is a 2.3-million-word compute shader** (a neural network, 720p to 1080p). The NVIDIA driver needs minutes to compile it and a synchronous compile on the replay thread stops every draw. `vk_exec_draw.c` `raiders_upscaler` replaces its dispatch with a linear blit, and `vk_pipeline.c` `big_cs_defer` builds any other compute shader of over 400,000 words on a thread of its own.
- **Cube maps are made as 2D arrays.** The sky and probe cube maps are created as 2D-array textures of 6 layers and sampled through cube descriptors. `vk_texture.c` gives such images `CUBE_COMPATIBLE` for this game only (the same flag on every square array broke AMD drivers in other titles), and `vk_exec_draw.c` `get_view` accepts a cube view of them. Without it the cube conversion pass read a black stand-in and the whole ambient lighting was missing.
- **Bananza's light-quad skip does not apply** (`light_quad_fault_skip`): those quads are this game's lighting.
- **Frame dumps.** `TOTK_DUMP=key` and F9 dump a frame; `TOTK_DUMP_PAIR=1` the next one too; `TOTK_DUMP_MIN_DRAWS=N` copies images only of passes with N draws or more (a full frame has 350 passes and the PNGs take minutes otherwise); `TOTK_DUMP_UBO=1` prints 32 uniform vectors per stage; `TOTK_DUMP_BUF=cs5` follows the buffer bound as compute SSBO slot 5. `TOTK_DUMP=probe` starts a dump by itself when the 78-layer reflection arrays are baked. The `hang:` lines the host writes after 10 s without a present name every guest thread, with its native stack.
- **Open: the picture flickers from frame to frame.** In two consecutive frames the geometry (depth) is identical but the shading is not: one frame has dark characters, or a character's hair is blue in one frame and green in the next. Two 82,000-byte storage buffers (a light list: a header with a count, then records of position, colour, direction and a next index) swap between slots 1 and 5 each frame; a compute shader writes one of them at the start of the frame and the lighting pass reads both. Their contents are valid in the snapshots, so the fault is not yet located. Presented frames alternate between 16 ms and 90 ms (about 19 fps), and 285 of the 350 passes of a frame are 880x880 full-screen passes of one small shader.

### Lifter gaps found and closed

The DKB lifter had all Switch 2 additions except two instructions used by this game's `sdk` (62 sites total):

- `PACGA` (ARMv8.3, 58 sites in `sdk`): returns 0, like the other pointer-authentication instructions, because the
  host never signs or authenticates pointers and the guest only compares hashes it computed itself.
- `FNEG`/`FABS` on eight half-precision lanes (`fneg v2.8h`, 4 sites in `sdk`): lowered to a 128-bit XOR/AND with
  `0x8000800080008000` per 64-bit half in `recomp/simd_c.py` (`misc2_fp16`).

`recomp/gen.py --exefs <update exefs> --out generated/raiders` now reports `0 unsupported` for all three modules, and
the generated C passes `clang -fsyntax-only -std=gnu11` with `runtime/` on the include path.

## Next steps

1. ~~**Build skeleton for this game.**~~ Done: `tools/build_game.py --version raiders` compiles the runtime against
   `generated/raiders` and links `raiders.exe`, without DKB's patch sets. A target with no
   `tools/data/expected-<target>.json` records the SHA-256 of its own ExeFS (`--exefs`), so nothing derived from the
   dump enters the repository.
2. ~~**Import survey.**~~ Done (see "Host surface"): no import needs a host implementation; the 14 outside
   `sdk`/`subsdk0` are weak and bind to NULL.
3. **First boot.** Keep generated code and the loaded ExeFS on the same version. Investigate the host access violation
   in `nnMain` after the RomFS mount and NVN/HID initialization, then continue toward the first frame.
4. ~~**Loose RomFS.**~~ Done: `tools/romfs_index.py` indexes the extracted Raiders directory and `runtime/romfs.c`
   opens files in place; no packed image or second multi-gigabyte copy is needed.
5. **NVN 2 survey.** Log every `nvnBootstrapLoader` table entry this game touches and every new command-buffer mode
   before recording anything, as DKB did for its 51 new NVN 2 calls.
6. **Shaders.** The seed census, xlate translator and Vulkan driver-cache preparation now run end to end. Continue investigating the
   30 unsupported Eglantine modes (29 byte-to-float and one multiply-wide addend), then rerun `raiders.exe --prepare-shaders`.

## Rules kept from DKB Recomp

- Generated C is never edited by hand; change `recomp/` and regenerate.
- `recomp/layout.py` alone decides module bases.
- Lifter changes come with the differential tests in `tests/diff` where the instruction space is synthetic.
- Nothing from the game enters the repository: no assets, no extracted archives, no shader blobs.
