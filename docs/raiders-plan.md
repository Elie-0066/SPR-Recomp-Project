# SPR Recomp (Splatoon Raiders) — plan

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

Other versions are experimental. The installer accepts the base (`tools/data/expected-raiders-base.json`) and any later
update of the game with the warning `warn.spr_version_experimental`; none of them has been lifted, built or run. The
runtime showed no address tied to 65536 in a search (its Raiders paths key on `rt_game_target` and on shader hashes), so the risks are
lifter gaps in code the update does not have, and SDK or NVN 2 calls a later update adds.

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
| P4 Shaders | ✅ | Eglantine lowers all 3,690 seed stages of `seed-nvn2.bin` (an offline run of `LowerToMaxwell` over the pack reported 0 failures). The last fixes: nested `CALL` linkage (the Maxwell front end inlines `CAL`) and `IMAD Rd, RZ, RZ, -c[..]` lowered as `MOV` from the constant buffer plus a negating `IADD` (`lower_to_maxwell.h`, cases 0x944 and 0x424/0x624). A failed lowering logs the stage kind and the instructions around the failing one (`nvn2_lower.cpp`). |
| P5 First frame | ✅ | Title, difficulty choice, the opening cutscene and the character creator render at about 55 fps. |
| P6 Gameplay | partial | The first gameplay zone loads and plays, at 20-25 fps before the write-back fix below, with camera stutter, flickering textures and the player's body missing (only clothes and head draw); all three fixed since. |


### Bring-up findings

Each one cost a session; they are listed so the next one does not repeat the search.

- **The upscaler is a 2.3-million-word compute shader** (a neural network, 720p to 1080p). The NVIDIA driver needs minutes to compile it and a synchronous compile on the replay thread stops every draw. `vk_exec_draw.c` `raiders_upscaler` replaces its dispatch with a linear blit, and `vk_pipeline.c` `big_cs_defer` builds any other compute shader of over 400,000 words on a thread of its own.
- **The upscaler ends with an sRGB decode.** Both variants of the network (fixed and dynamic resolution) read the frame sRGB-encoded and finish with `x < 0.04045 ? x / 12.92 : ((x + 0.055) / 1.055)^2.4`, so the passes after it get linear colour. The linear blit that stood in for it left the values encoded: gameplay, which upscales 1536x864 to 1920x1080, looked washed out and soft, while scenes rendered at 1080p did not. `vk_exec_draw.c` `raiders_upscale` now runs `shaders/raiders_upscale.comp` instead (FSR 1 EASU on the encoded frame, then the decode); `DKB_RAIDERS_UPSCALE=blit` restores the blit.
- **Cube maps are made as 2D arrays.** The sky and probe cube maps are created as 2D-array textures of 6 layers and sampled through cube descriptors. `vk_texture.c` gives such images `CUBE_COMPATIBLE` for this game only (the same flag on every square array broke AMD drivers in other titles), and `vk_exec_draw.c` `get_view` accepts a cube view of them. Without it the cube conversion pass read a black stand-in and the whole ambient lighting was missing.
- **Bananza's light-quad skip does not apply** (`light_quad_fault_skip`): those quads are this game's lighting.
- **Frame dumps.** `TOTK_DUMP=key` and F9 dump a frame; `TOTK_DUMP_PAIR=1` the next one too; `TOTK_DUMP_MIN_DRAWS=N` copies images only of passes with N draws or more (a full frame has 350 passes and the PNGs take minutes otherwise); `TOTK_DUMP_UBO=1` prints 32 uniform vectors per stage; `TOTK_DUMP_BUF=cs5` follows the buffer bound as compute SSBO slot 5. `TOTK_DUMP=probe` starts a dump by itself when the 78-layer reflection arrays are baked. The `hang:` lines the host writes after 10 s without a present name every guest thread, with its native stack.
- **The 880x880 passes are the hub's ink.** FS `3f6af34afb0c405a` (VS `245a619125222d33`) is drawn about 285 times per frame into 880x880 targets in the character creator and the hub; in the levels the same pair draws into targets of other sizes, 90-170 times per frame. `vk_exec_draw.c` `raiders_skip_880` used to skip it at 880x880, when it held the character creator at 18-19 fps, and the hub's ground then took no ink. After the later fixes drawing it costs no measurable frame rate, so it is drawn by default; `TOTK_RAIDERS_SKIP_880=1` skips it again.
- **GPU profile.** `DKB_GPU_PROFILE=1` (`vk_gpuprof.c`) writes a timestamp after every draw, dispatch and op and logs every 10 s the shader pairs and ops that cost the most GPU time per frame. On NVIDIA a timestamp does not wait for the draws before it, so mode 1 charges a frame of rendering to the first full barrier (it showed up as "mirror write-back"). `DKB_GPU_PROFILE=2` ends the pass and drains the GPU before every timestamp: slow, but each entry is then the real cost. Do not leave mode 2 set in a PowerShell window used for normal play. `perf.c` adds "GPU busy" per frame to the frame-time report and `vk_sync.c` a 10 s submission-latency report.
- **Write-back by difference (the gameplay frame rate).** In gameplay one compute shader (`996db3ac4cf84b14`, one thread) writes a few 112-byte records through a storage pointer of unknown size, so the binding reaches the end of its pool: 53 MiB of the VRAM mirror went back to guest memory every frame, 10-18 ms of GPU time over the PCIe 3.0 bus of the test machine (Ryzen 5 4500, RTX 3050). `vk_memory.c` "write-back by difference" snapshots a written span of 8 MiB or more bound by a dispatch; at the end of the submission `shaders/mirror_diff.comp` stores only the 16-byte blocks that changed. Smaller spans are usually rewritten whole and the copy engine moves them faster (a 256 KiB threshold cost the dialogue scene 20 fps). The first version crashed the GPU (a write at address 0) on entering gameplay; the current one skips pools whose buffers are gone and no longer crashes. `DKB_MIRROR_DIFF=0` turns it off. `vk_memory.c` `wb_tally` still logs the sources of any submission that writes back 8 MiB or more.
- **Ruled out for the flicker:** stale VRAM mirrors (`TOTK_MEMORY=verify` found 0 mismatches, `TOTK_MEMORY=import` still flickers), completion-thread latency (side effects under 0.05 ms per wake), and the formerly untranslated material shaders (all translate now, no visual change).
- **The flicker, the green tint and the camera stutter were one race** (fixed). The game writes the next frame's data right after a present with no fence, relying on the console's GPU having read the frame already. Here the GPU finishes the frame up to 25 ms after the submit, so in the dialogue scene every other frame was dark with Pasquale and the clothes tinted green, gameplay textures blinked, and frame times alternated (16/70 ms), which is the camera stutter. `nvn_api_core.c` `nvn_submit_wait_mode` makes `nvnQueuePresentTexture` return only once the GPU finished everything submitted before it (`DKB_SUBMIT_WAIT=2`, the default for this target; `=1` waits after every submit and costs gameplay about 4 fps; `=0` turns it off). Measured on 600 flight-recorder thumbnails of the dialogue: the 90th percentile of the frame-to-frame difference went from 45 to 5. Slowing frames down hides the flicker too, which is why full-size `TOTK_SHOTS` series showed none.
- **Deferred uniform updates.** About 1,100-1,250 `UPDATE_UB` per gameplay frame each ended the render pass and drained the GPU twice. `vk_exec_draw.c` (see `UBD`, `ub_shadow`, `vk_exec_ub_flush`) now binds an updated block as a transient copy (the block's submit-time snapshot with the updates applied in order) and writes the updates into memory at the next pass break (`vk_exec.c` `barrier_to`). All updates are deferred in gameplay (the log line "uniform updates: N deferred"); `DKB_UB_DEFER=0` restores the old path. `DKB_GPU_PROFILE=2` cannot show the gain: its per-op barrier flushes the deferred copies.
- **The ink at the mission boss (30 fps).** In missions the ink is painted into a 6400x6400 map, one pass per splat with an `UPDATE_UB` of the splat's block before it. With many enemies one submission carries up to 7,985 updates, past the 4,096 entries of the update ring (`vk_exec_draw.c` `UPD_N`), so every later update went back to the in-place path: up to 1,958 a frame, each ending the ink pass behind a full barrier, and the boss held 30-45 fps with up to 24 ms of GPU time a frame; with the larger ring it runs at full rate. The ring holds 32,768 now. With `TOTK_TRACE=perf` the log names what ended the render passes every 600 frames (`vk_exec.c` `pe_note`: "pass ends per frame", all passes and those of 4096 pixels or more) and why uniform updates were written in place (`vk_exec_draw.c` `ubw_note`: ring full, over 64 KiB, or the snapshot's reason from `vk_usnap.c` `vk_usnap_why`).
- **Why gameplay held 30 fps, and the fix (28-31 fps to 56-59).** `DKB_TIMELINE=<present>` (`perf.c` `perf_tl`) logs four frames of submits, sync waits, GPU completions, presents and vsync ticks; `DKB_GPU_PROFILE=3` (`vk_exec.c` `gp3_stamp`) times each render pass and dispatch with one drain at its end, and charges the GPU's idle time to "(between command buffers)" and "(between submissions)". In the first zone the passes cost about 8 ms, but the GPU spent 20 ms a frame between command buffers: the VRAM mirror re-uploaded 56 MiB a frame. Its source was a round trip: `vk_exec_draw.c` reports every GPU-written range at completion (`vk_gpu_written`), which stamped the pages like a CPU write, and the next submission copied the 53 MiB storage span (pool `0x582f87000`, flags 0x24) from guest memory back into the mirror it came from. `vk_memory.c` `vk_gpu_written` now gives such writes a generation of its own (`gpu_gen`) that `mirror_sync` skips, unless the page also has a CPU write the mirror has not copied; uploads fell to 0.36 MiB a frame and `TOTK_MEMORY=verify` compared 6 million pages with no mismatch. `DKB_MIRROR_GPU_SKIP=0` restores the old stamps. The log line "mem: mirror upload source" names the pools of any window that uploads 4 MiB or more per submission.
- **Uniform copies sized to what the shader reads.** The deferred updates bound 64 KiB transient copies (the translator declares every bank as 4096 vec4), about 70 MiB of `memcpy` a frame on the render thread. `vk_pipeline.c` `ubo_read_bytes` reads from the SPIR-V the extent each uniform block is read at when all its indices are constant (506 of 960 blocks), and `ub_shadow` copies only that: 0.3 KiB per copy on average. `DKB_UBO_READ=0` copies the whole bound range.
- **The `tex_lock` wait at each submit (fixed).** `DKB_TEXLOCK_PROF=1` (`vk_texture.c` `lock_at`) logs every 600 presents, per function, how long `tex_lock` is held and waited for. The render thread waited 1.3-1.5 ms a frame in `vk_texture_new_submit` while a game thread held the lock in `vk_texture_write_back_small`: after each fence wait it read two small render targets back through the upload batch, behind all the GPU work queued since. Skipping the lock when busy, or releasing it during that readback, brought the flicker back in the dialogue: the next submission must not be recorded before the storage holds the render. Now a target that went back once is copied to a host buffer right after each of its renders, in the same command buffer (`vk_texture_prefetch_flush`, from `vk_exec.c` `vk_exec_pass_end`), so the write-back at the fence is a copy into storage under the same lock and in the same order: hold time 0.06 ms a frame, the submit's wait 0.05 ms, 99.9% of the write-backs from the prefetched copies. `DKB_WB_PREFETCH=0` reads back at the fence as before.
- **Staged mirror uploads, and what they did not fix.** `vk_memory.c` `stage_ranges` copies the pages a mirror needs on the CPU at record time instead of reading guest memory when the GPU runs the copy (`DKB_MIRROR_STAGE=0` turns it off; at most 8 MiB per prologue, a new pool's first whole copy still goes straight). With it, `DKB_SUBMIT_WAIT=0` still flickers, so the present wait stays; it also costs less than it seemed (16.9 against 17.6 ms a frame without it). `DKB_MIRROR_DIFF_MIN=<KiB>` lowers the write-back-by-difference threshold for experiments.
- **The GPU fault on entering gameplay (fixed, to be confirmed over more runs).** Runs ended with a device fault (a write at 0, or a read at an address no buffer ever had) right after "write-back by difference on". `DKB_GPU_CHECKPOINTS=1` with `TOTK_CRUMB_BIND=1` put it in the difference dispatch itself (`vk_memory.c` `diff_run`, named "MIRRDIFF" in the report): it pushed its descriptors through the device function pointer instead of `rec_push`, so the push went into the replay part on the replaying thread while the recorder thread (`vk_rec.h`) was still making the commands queued before it. The more work queued at that point, the likelier the collision: the upscale shader made it happen on nearly every run.
- **The player's missing body (fixed).** The characters' colour pass draws with depth EQUAL against their own depth pass, and the body was absent from that depth pass: its draw (`vs 073923a348551a68`) alpha-tests an opacity mask, a 128x128 BC4 texture the game builds at the outfit's load. It draws two masks into R8 targets, combines them with two BC4 textures and encodes the result through an RGBA16UI view of the mask's blocks, one pass per level. One of the two inputs sits in a pool with `CPU_NO_ACCESS` (flags 0x21) whose storage holds the game's data; `vk_texture.c` `tex_mirrored` made every texture of such a pool GPU-owned, so it was never uploaded, the combined mask was zero and the body's alpha test discarded it. Block-compressed textures of `CPU_NO_ACCESS` pools that are not compressible are now mirrored. Found with `TOTK_DUMP=bcview` (the dump starts at the first draw into a BC block view), `TOTK_DUMP_VBUF=1` and `DKB_DUMP_UBO=<n>`.

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
   `generated/raiders` and links `spr-recomp.exe`, without DKB's patch sets. A target with no
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
6. ~~**Shaders.**~~ Done: every seed stage lowers (see P4). Rerun `spr-recomp.exe --prepare-shaders` after lowering changes.
7. **Gameplay frame rate.** About 59-60 fps in the first zone at vsync (1% low about 42). What is left: frames of 35-50 ms
   every few seconds whose time is mostly outside the replay (`TOTK_PROFILE_SLOW=<ms>` dumps every thread's chains of
   such a frame), and the main submission's replay (8-10 ms on the game's render thread).
8. ~~**The missing body.**~~ Done (see P6).

## Rules kept from DKB Recomp

- Generated C is never edited by hand; change `recomp/` and regenerate.
- `recomp/layout.py` alone decides module bases.
- Lifter changes come with the differential tests in `tests/diff` where the instruction space is synthetic.
- Nothing from the game enters the repository: no assets, no extracted archives, no shader blobs.
