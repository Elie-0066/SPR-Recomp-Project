# Splatoon Raiders Recomp — plan

Static recompilation of *Splatoon Raiders* (Nintendo Switch 2) into a native Windows x86-64 executable, built on
[DKB Recomp](https://github.com/Lucas-blip-png/dkb-pc-recomp), which is itself built on
[TotK Recomp](https://github.com/valentderah/totk-pc-recomp). No emulator runs at play time: the game's ARM64 code
is lifted to C ahead of time, compiled with Clang, and its NVN 2 graphics calls are answered by a host implementation
that renders through Vulkan.

No game files are in this repository and none will be. The build reads the user's own dump from
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
- Shaders are precompiled in the RomFS: `Shader\Hoian_UBER.Product.100.product.Nin_Ounce_NVN2.bfsha.zs` (10.1 MB
  compressed) and `Shader\Sample.Nin_Ounce_NVN2.release.sarc.zs` (9.2 MB compressed → SARC with SFAT/SFNT tables,
  12 entries whose payloads are `FSHA` shader archives, `AAMP` parameter blocks and `BAHS`/`SHAB`-style binaries).
  Both are raw zstd frames (`28 B5 2F FD`). DKB's Ampere lowering and the Ryujinx translator are the intended path;
  what is missing is an `FSHA` reader that feeds the same stage data DKB got from its shader cache.
- `main` imports `movie.nss`; the sdk module exports the `nn::movie` classes. The host needs the codec surface
  DKB implemented for VP9 (`runtime/ov_codec.c`, libvpx).

## Status

| Phase | | State |
|---|---|---|
| P0 Dump intake | ✅ | Loose ExeFS/RomFS read in place; no keys needed. NSO0 header parsed; module name and build ids read. |
| P1 Lifter | ✅ | 259,898 functions lifted with **0 unsupported instructions**: `main` 212,391, `sdk` 45,972, `subsdk0` 1,535. 1,107 MB + 151 MB + 9 MB of C. |
| P2 Horizon host | 🚧 | Nothing done yet: threads, `svc*`, `nn::os`/`fs`/`oe`/`am`, RomFS mount, input, audio, save. |
| P3 NVN 2 front end | ⬜ | Reuse DKB's 700 entry points; survey which ones this game calls and which new command-buffer modes it uses. |
| P4 Shaders | ⬜ | `FSHA` reader + Ampere→Maxwell→SPIR-V for the archive in `romfs\Shader`. |
| P5 First frame | ⬜ | Boot `nnMain`, reach the title screen, then the first playable scene. |

### Lifter gaps found and closed

The DKB lifter had all Switch 2 additions except two instructions used by this game's `sdk` (62 sites total):

- `PACGA` (ARMv8.3, 58 sites in `sdk`): returns 0, like the other pointer-authentication instructions, because the
  host never signs or authenticates pointers and the guest only compares hashes it computed itself.
- `FNEG`/`FABS` on eight half-precision lanes (`fneg v2.8h`, 4 sites in `sdk`): lowered to a 128-bit XOR/AND with
  `0x8000800080008000` per 64-bit half in `recomp/simd_c.py` (`misc2_fp16`).

`recomp/gen.py --exefs <base exefs> --out generated/raiders` now reports `0 unsupported` for all three modules, and
the generated C passes `clang -fsyntax-only -std=gnu11` with `runtime/` on the include path.

## Next steps

1. **Build skeleton for this game.** `tools/build_game.py` is TotK/DKB-specific (version names, expected hashes, exe
   name, RomFS overlays, shader preparation). Add a `raiders` version path that compiles the same runtime against
   `generated/raiders` and links `raiders.exe`, without the DKB patch sets. Keep the change additive so upstream
   fixes still apply.
2. **Import survey.** Classify the 1,270 `main` imports (nn::os, nn::fs, nn::oe, nn::am, nn::gfx, nn::codec, libc,
   C++ ABI) and cross them with the host's symbol table, so the boot log names the first missing symbol instead of
   trapping in the middle of the loader.
3. **NVN 2 survey.** Log every `nvnBootstrapLoader` table entry this game touches and every new command-buffer mode
   before recording anything, as DKB did for its 51 new NVN 2 calls.
4. **Boot to `nnMain`.** RomFS mount, heap, thread setup, `nn::oe` initialisation; then the title screen.
5. **Shaders.** Decode one `FSHA` archive, dump its stage list, and check the SASS encoding (Ampere SM87/88) against
   Eglantine's lowering before wiring it to `tools/xlate`.

## Rules kept from DKB Recomp

- Generated C is never edited by hand; change `recomp/` and regenerate.
- `recomp/layout.py` alone decides module bases.
- Lifter changes come with the differential tests in `tests/diff` where the instruction space is synthetic.
- Nothing from the game enters the repository: no assets, no extracted archives, no shader blobs.
