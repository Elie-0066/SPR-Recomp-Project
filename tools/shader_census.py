"""Census of Maxwell shaders and Splatoon Raiders NVN 2 stages, with an offline translation pass.

Port of MacNativeGames games/totk/scripts/prepare_shader_seed.py (inventory, bnsh_shaders, sharcb_shaders, sarc) and the
package walk of locate_shaders.py @ b75be8cf (MIT, Copyright (c) 2026 MacNativeGames contributors). Differences: no
shared shader list, no spirv-cross, no bind-state variants; the module scan reads the NSO files under exefs/ directly;
a control section must start with CONTROL_MAGIC (sharcb_shaders says why).

Reads build/extracted/<version>/ by default. `--romfs` and `--exefs` select other inputs; `--romfs-base` combines a loose
base RomFS with a loose update overlay. Writes under build/<version>/shaders/ only:
    code/<key>.code     translator input (graphics: SPH + instructions, compute: instructions)
    jobs.json           key -> job, source path, origin (bnsh|sharcb|main)
    seed-nvn2.bin       DKBN2SD1 pack for embedded Raiders FSHA/BNSH stages
    spv/<key>.spv       with --translate
    failures.txt        with --translate, when something failed: groups by message

The existing shader key is sha256(control[:0x880] + code). Python 3.14 uses compression.zstd; Python 3.12 uses the
installed zstandard package.
"""
import argparse
try:
    import compression.zstd as zstd
except ImportError:
    # Python 3.12 build environment; the extracted Raiders RomFS can be read without
    # installing the Python 3.14-only compression.zstd module.
    import zstandard as _zstandard

    class _ZstdCompat:
        @staticmethod
        def decompress(data, zstd_dict=None):
            dictionary = _zstandard.ZstdCompressionDict(zstd_dict) if zstd_dict is not None else None
            return _zstandard.ZstdDecompressor(dict_data=dictionary).decompress(data)

        @staticmethod
        def get_frame_info(data):
            info = _zstandard.get_frame_parameters(data)
            size = info.content_size
            return type('FrameInfo', (), {
                'decompressed_size': size if size != _zstandard.CONTENTSIZE_UNKNOWN else None,
                'dictionary_id': info.dict_id,
            })()

        @staticmethod
        def ZstdDict(data):
            return _zstandard.ZstdCompressionDict(data)

    zstd = _ZstdCompat()
import concurrent.futures
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import time
from collections import defaultdict
from pathlib import Path

import lz4.block

ROOT = Path(__file__).resolve().parent.parent
XLATE = ROOT / 'build' / 'xlate' / ('totk_xlate.exe' if os.name == 'nt' else 'totk_xlate')
SPIRV_VAL = Path(r'C:\msys64\ucrt64\bin\spirv-val.exe') if os.name == 'nt' else Path(shutil.which('spirv-val') or '/usr/bin/spirv-val')
ZSTD = b'\x28\xb5\x2f\xfd'
# NVN program control section magic, u32 0x98761234 (d2r-mobile dm/gfx/tools/shader_map.py)
CONTROL_MAGIC = b'\x34\x12\x76\x98'
STAGE_IDS = {0: 'vs', 1: 'ps', 5: 'cs'}
MAX_WORKERS = 20


def digest(data):
    return hashlib.sha256(data).hexdigest()


def checked(data, offset, size):
    if offset < 0 or size < 0 or offset + size > len(data):
        raise ValueError('shader container range outside file')
    return data[offset:offset + size]


def sarc(data):
    if data[:4] != b'SARC':
        raise ValueError('expected SARC')
    endian = '<' if data[6:8] == b'\xff\xfe' else '>'
    header, size, start = struct.unpack_from(endian + 'HxxII', data, 4)
    if size != len(data) or header < 20 or checked(data, header, 4) != b'SFAT':
        raise ValueError('invalid SARC header')
    count = struct.unpack_from(endian + 'H', data, header + 6)[0]
    names = header + 12 + count * 16
    if checked(data, names, 4) != b'SFNT':
        raise ValueError('invalid SARC string table')
    names += 8
    result = {}
    for i in range(count):
        _, attribute, lo, hi = struct.unpack_from(endian + '4I', checked(data, header + 12 + i * 16, 16))
        off = names + (attribute & 0xffffff) * 4
        if attribute >> 24 != 1 or not names <= off < start or hi < lo or start + lo < start:
            raise ValueError('invalid SARC member')
        name = data[off:data.index(0, off, start)].decode('utf-8')
        if name.startswith('/') or '\\' in name or any(p in ('', '.', '..') for p in name.split('/')) or name in result:
            raise ValueError('unsafe or duplicate SARC name')
        result[name] = checked(data, start + lo, hi - lo)
    return result


def bnsh_shaders(data):
    """BNSH absolute offsets are relative to each embedded BNSH, not BFSHA."""
    if data[:4] != b'BNSH' or checked(data, 12, 2) != b'\xff\xfe' or checked(data, 96, 4) != b'grsc':
        raise ValueError('unsupported BNSH header')
    size = struct.unpack_from('<I', data, 28)[0]
    data = checked(data, 0, size)
    count, variations = struct.unpack_from('<IQ', data, 124)
    checked(data, variations, count * 64)
    for i in range(count):
        program = struct.unpack_from('<Q', data, variations + i * 64 + 16)[0]
        checked(data, program, 56)
        for stage, relative in [('vs', 8), ('ps', 40), ('cs', 48)]:
            offset = struct.unpack_from('<Q', data, program + relative)[0]
            if not offset:
                continue
            control_off, code_off, code_size, control_size = struct.unpack_from('<QQII', checked(data, offset + 8, 24))
            control = checked(data, control_off, control_size)
            code = checked(data, code_off, code_size)
            if len(control) < 0x880:
                continue   # the SDK utility BNSH uses an older, shorter control layout
            control = control[:0x880]   # some are 0x88c; NVN and the runtime's key read 0x880
            if control[:4] != CONTROL_MAGIC:
                raise ValueError('NVN control section without its magic')
            c1, length = struct.unpack_from('<II', control, 0x700)
            actual = struct.unpack_from('<I', control, 0x714)[0]
            if actual != {'vs': 0, 'ps': 1, 'cs': 5}[stage] or not 0 <= c1 <= length <= len(code) or not length:
                raise ValueError('invalid NVN shader bounds or stage')
            yield digest(control + code[:length]), stage, control, code[:length]


def bnsh_nvn2_stages(data):
    """Read the short-control NVN 2 stages stored in Raiders' FSHA/BNSH archives."""
    if data[:4] != b'BNSH' or checked(data, 12, 2) != b'\xff\xfe' or checked(data, 96, 4) != b'grsc':
        raise ValueError('unsupported Raiders BNSH header')
    size = struct.unpack_from('<I', data, 28)[0]
    data = checked(data, 0, size)
    count, variations = struct.unpack_from('<IQ', data, 124)
    checked(data, variations, count * 64)
    for i in range(count):
        program = struct.unpack_from('<Q', data, variations + i * 64 + 16)[0]
        checked(data, program, 56)
        for stage, relative in ((0, 8), (4, 40), (5, 48)):
            offset = struct.unpack_from('<Q', data, program + relative)[0]
            if not offset:
                continue
            control_off, code_off, code_size, control_size = struct.unpack_from(
                '<QQII', checked(data, offset + 8, 24))
            if control_size >= 0x880:
                continue  # the existing census handles the older Maxwell control layout
            control = checked(data, control_off, control_size)
            code = checked(data, code_off, code_size)
            if not control or not code:
                continue
            key = digest(struct.pack('<I', stage) + control + code)
            yield key, stage, control, code


def fsha_nvn2_stages(data):
    """Yield short-control BNSH programs embedded in an FSHA file."""
    if data[:4] != b'FSHA':
        return
    pos = 0
    while (pos := data.find(b'BNSH\0\0\0\0', pos)) >= 0:
        size = struct.unpack_from('<I', checked(data, pos + 28, 4))[0]
        if size < 128:
            raise ValueError('invalid BNSH size in FSHA')
        for shader in bnsh_nvn2_stages(checked(data, pos, size)):
            yield shader
        pos += size


def sharcb_shaders(data, rejected):
    """agl shader packages (.sharcb, magic BAHS): records <u64 code offset in the pool, u32 control size (0x880 or
    0x88c), i32 offset of the control from the record>; a record may reuse an earlier record's control.

    The scan finds records by their size word, so it also meets 16-byte groups that only look like one. Without the
    magic check, 1.2.1 yields 87 such keys: each has control offset 0 (the "control" starts at the record itself, so its
    first 8 bytes are the code offset) and no CONTROL_MAGIC. The game hands NVN real control sections and the runtime
    key hashes those, so no draw can ever produce these keys. Their code is cut from the wrong place: they were all 67
    translation failures of 1.2.1 (IndexOutOfRange in ShaderHeader.ctor and ArgumentOutOfRange in Decoder.FillBlock,
    on 0 to 4,718 bytes of code). MNG's scan (prepare_shader_seed.py @ b75be8cf) keeps them; the 7 that translate are
    V rows in MNG's list only because shader_list.py add_cache() folds the seed's own .variants files back in. A
    candidate without CONTROL_MAGIC is skipped and counted in rejected[0]."""
    if len(data) < 32 or data[:4] != b'BAHS':
        raise ValueError('expected an agl shader package')
    start, pool_size, pool = struct.unpack_from('<3I', data, 20)
    if not 32 <= start <= pool or pool + pool_size > len(data):
        raise ValueError('agl shader package layout outside the file')
    seen = set()
    for size in (b'\x80\x08\x00\x00', b'\x8c\x08\x00\x00'):
        at = start
        while (at := data.find(size, at, pool)) >= 0:
            record, at = at - 8, at + 4
            if record < start or record % 4:
                continue
            code_off = struct.unpack_from('<Q', data, record)[0]
            control_at = record + struct.unpack_from('<i', data, record + 12)[0]
            if not start <= control_at <= pool - 0x880 or code_off >= pool_size:
                continue
            control = data[control_at:control_at + 0x880]
            if control[:4] != CONTROL_MAGIC:
                rejected[0] += 1
                continue
            c1, length = struct.unpack_from('<II', control, 0x700)
            stage = STAGE_IDS.get(struct.unpack_from('<I', control, 0x714)[0])
            if not stage or not 0 < length <= pool_size - code_off or c1 > length:
                continue
            code = data[pool + code_off:pool + code_off + length]
            key = digest(control + code)
            if key not in seen:
                seen.add(key)
                yield key, stage, control, code


def nso_image(path):
    """The decompressed text, rodata and data segments of an NSO, concatenated (enough to find embedded BNSH files)."""
    raw = path.read_bytes()
    if raw[:4] != b'NSO0':
        raise ValueError(f'{path} is not an NSO')
    flags = struct.unpack_from('<I', raw, 0xC)[0]
    parts = []
    for i in range(3):
        file_off, _, dec_size = struct.unpack_from('<III', raw, 0x10 + i * 0x10)
        comp_size = struct.unpack_from('<I', raw, 0x60 + i * 4)[0]
        data = raw[file_off:file_off + comp_size]
        parts.append(lz4.block.decompress(data, uncompressed_size=dec_size) if flags & (1 << i) else data)
    return b''.join(parts)


def startup_package(name):
    """The RomFS files MNG's startup shaders come from; effects and UI layouts carry more."""
    return (name == 'Lib/NintendoSDK/TextureShader.bnsh' or name == 'Pack/Bootup.Nin_NX_NVN.pack.zs' or
            name.startswith('Shader/') and name.endswith('.zs'))


# The RomFS directories that can hold shaders: the agl and gsys packages, the effects, terrain, the UI layout archives
# and the game's own shader packs. Every shader of 1.1.1 (27,576) comes from these six; they are 1,996 entries and
# 147 MiB, 0.9% of a RomFS whose every file the walk below used to read and decompress first (65 s against 2.8 s).
SHADER_DIRS = ('Shader/', 'Effect/', 'Lib/agl/', 'Lib/Terrain/', 'Lib/gsys/', 'UI/LayoutArchive/')


def inventory(romfs, exefs, everything=True, base_romfs=None):
    """Every unique shader: key -> dict(stage, control, code, source, origin, startup). As in MNG, a shader is a startup
    shader when it comes from an agl package, a game module, or a startup_package() RomFS file; the others come from
    effect and UI layout resources, which MNG translates only for bind states players have met. Also returns the number
    of .sharcb record candidates rejected for a missing control magic (see sharcb_shaders)."""
    loose = romfs.is_dir() and not (romfs / 'index.json').is_file()
    if loose:
        entries = {}
        if base_romfs:
            entries.update({p.relative_to(base_romfs).as_posix(): p for p in base_romfs.rglob('*') if p.is_file()})
        entries.update({p.relative_to(romfs).as_posix(): p for p in romfs.rglob('*') if p.is_file()})
    else:
        index = json.loads((romfs / 'index.json').read_text(encoding='utf-8'))
        if index.get('status') != 'pass':
            raise ValueError('RomFS has not passed integrity verification')
        entries = {r['path']: r for r in index['files']}
    shaders = {}
    nvn2_stages = {}
    rejected = [0]

    def add(origin, name, found, startup):
        for key, stage, control, code in found:
            s = shaders.setdefault(key, dict(stage=stage, control=control, code=code, source=name, origin=origin,
                                             startup=startup))
            if startup and not s['startup']:
                s.update(source=name, origin=origin, startup=True)

    stream_context = open(os.devnull, 'rb') if loose else (romfs / 'romfs.bin').open('rb')
    with stream_context as stream:
        def read(name):
            if loose:
                return entries[name].read_bytes()
            row = entries[name]
            stream.seek(row['offset'])
            raw = stream.read(row['size'])
            if len(raw) != row['size']:
                raise ValueError('short RomFS read')
            return raw

        dictionaries = {}
        if 'Pack/ZsDic.pack.zs' in entries:
            for raw in sarc(zstd.decompress(read('Pack/ZsDic.pack.zs'))).values():
                d = zstd.ZstdDict(raw)
                dictionaries[d.dict_id] = d

        def decode(raw):
            info = zstd.get_frame_info(raw)
            if info.decompressed_size is None or info.decompressed_size > 512 << 20:
                raise ValueError('unbounded shader package')
            dictionary = dictionaries.get(info.dictionary_id) if info.dictionary_id else None
            if info.dictionary_id and dictionary is None:
                raise ValueError(f'missing zstd dictionary {info.dictionary_id}')
            try:
                return zstd.decompress(raw, zstd_dict=dictionary)
            except Exception as exc:
                raise ValueError(f'zstd decode failed for {name}: {exc}') from exc

        def visit(origin, name, data, startup, depth=0):
            if data[:4] == b'BAHS':
                add('sharcb', name, sharcb_shaders(data, rejected), True)
                return
            if data[:4] == b'SARC':
                if depth > 4:
                    return
                for member, raw in sarc(data).items():
                    visit(origin, name + '/' + member, decode(raw) if raw[:4] == ZSTD else raw, startup, depth + 1)
                return
            if data[:4] == b'FSHA':
                for key, stage, control, code in fsha_nvn2_stages(data):
                    nvn2_stages.setdefault(key, (stage, control, code, name))
            pos = 0   # BFSHA and BNSH files, effect and layout resources: complete, independently addressed BNSH files
            while (pos := data.find(b'BNSH\0\0\0\0', pos)) >= 0:
                size = struct.unpack_from('<I', checked(data, pos + 28, 4))[0]
                add(origin, name, bnsh_shaders(checked(data, pos, size)), startup)
                pos += max(size, 8)

        for name in sorted(entries):
            if not everything and not name.startswith(SHADER_DIRS):
                continue
            if name.endswith('.part'):
                continue  # incomplete chunks are not independently decodable zstd frames
            raw = read(name)
            if raw[:4] not in (ZSTD, b'SARC', b'FSHA', b'BNSH', b'BAHS') and b'BNSH\0\0\0\0' not in raw:
                continue
            visit('bnsh', name, decode(raw) if raw[:4] == ZSTD else raw, startup_package(name))
    for module in sorted(exefs.iterdir()):
        if module.name in ('main', 'sdk', 'subsdk0', 'rtld'):
            visit('main', 'module:' + module.name, nso_image(module), True)
    return shaders, rejected[0], nvn2_stages


def write_nvn2_seed(stages, path):
    """Write the NVN 2 seed format consumed by gfx_shader.c --prepare-shaders."""
    out = bytearray(b'DKBN2SD1') + bytearray(struct.pack('<I', len(stages)))
    for _, (stage, control, code, _) in sorted(stages.items()):
        out += struct.pack('<III', stage, len(control), len(code))
        out += control
        out += b'\0' * (-len(control) & 3)
        out += code
        out += b'\0' * (-len(code) & 3)
    path.write_bytes(out)


def write_shaders(shaders, out, everything):
    """Write code files and jobs.json; returns {key: dict(job, source, origin)}."""
    (out / 'code').mkdir(parents=True, exist_ok=True)
    jobs = {}
    for key, s in sorted(shaders.items()):
        if not s['startup'] and not everything:
            continue
        control, code, stage = s['control'], s['code'], s['stage']
        c1, length = struct.unpack_from('<II', control, 0x700)
        body = code[0 if stage == 'cs' else 0x30:length]
        path = out / 'code' / f'{key}.code'
        if not path.is_file() or path.read_bytes() != body:
            path.write_bytes(body)
        job = dict(key=key, stage=stage, code=str(path), c1=code[c1:length].hex(), debug=False)
        if stage == 'cs':
            job['local'] = list(struct.unpack_from('<III', control, 0x748))
        jobs[key] = dict(job=job, source=s['source'], origin=s['origin'], startup=s['startup'])
    (out / 'jobs.json').write_text(json.dumps(jobs, indent=0), encoding='utf-8')
    return jobs


def worker(keys, jobs, spv_dir):
    """One --serve process for a slice of keys. Returns (translated, failures {key: message}, errors {key: message})."""
    failures, errors, ok = {}, {}, 0
    process = subprocess.Popen([str(XLATE), '--serve'], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=subprocess.DEVNULL, text=True)
    try:
        for key in keys:
            process.stdin.write(json.dumps({'job': jobs[key]['job'], 'out': str(spv_dir)}) + '\n')
            process.stdin.flush()
            reply = process.stdout.readline().rstrip('\n')
            if not reply:
                failures[key] = 'translator service exited'
                break
            if not reply.startswith('ok '):
                failures[key] = reply.split(' ', 2)[2] if reply.count(' ') >= 2 else reply
                continue
            v = subprocess.run([str(SPIRV_VAL), '--target-env', 'vulkan1.3', str(spv_dir / f'{key}.spv')],
                               capture_output=True, text=True)
            if v.returncode:
                text = (v.stderr or v.stdout).strip()
                errors[key] = text.splitlines()[0] if text else 'spirv-val failed'
            else:
                ok += 1
    finally:
        process.stdin.close()
        process.wait()
    return ok, failures, errors


def normalize(message):
    return re.sub(r'\b\d+\b|0x[0-9a-fA-F]+', 'N', message)[:300]


def translate(jobs, out):
    spv_dir = out / 'spv'
    spv_dir.mkdir(exist_ok=True)
    keys = sorted(jobs)
    count = max(1, min(MAX_WORKERS, len(keys)))
    slices = [keys[i::count] for i in range(count)]
    ok, failures, errors = 0, {}, {}
    with concurrent.futures.ThreadPoolExecutor(count) as pool:
        for n, f, e in pool.map(lambda s: worker(s, jobs, spv_dir), slices):
            ok += n
            failures.update(f)
            errors.update(e)
    if failures or errors:
        lines = []
        for title, table in (('translate failures', failures), ('spirv-val errors', errors)):
            groups = defaultdict(list)
            for key, message in table.items():
                groups[normalize(message)].append((key, message))
            lines.append(f'== {title}: {len(table)} in {len(groups)} groups')
            for _, members in sorted(groups.items(), key=lambda g: -len(g[1])):
                lines.append(f'{len(members)}\t{members[0][1][:300]}')
                lines.extend(f'\texample {key}' for key, _ in sorted(members)[:2])
        (out / 'failures.txt').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    else:
        (out / 'failures.txt').unlink(missing_ok=True)
    return ok, len(failures), len(errors)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--version', required=True, help='version name; inputs default to build/extracted/<version>')
    ap.add_argument('--romfs', type=Path, help='RomFS image directory or loose extracted RomFS directory')
    ap.add_argument('--romfs-base', type=Path, help='base loose RomFS directory for an update overlay')
    ap.add_argument('--exefs', type=Path, help='ExeFS directory (defaults to build/extracted/<version>/exefs)')
    ap.add_argument('--all', action='store_true', help='also the layout and effect shaders MNG leaves to bind-state variants')
    ap.add_argument('--translate', action='store_true', help='translate every shader and run spirv-val')
    args = ap.parse_args()
    t0 = time.monotonic()
    extracted = ROOT / 'build' / 'extracted' / args.version
    out = ROOT / 'build' / args.version / 'shaders'
    out.mkdir(parents=True, exist_ok=True)
    shaders, rejected, nvn2_stages = inventory(args.romfs or extracted / 'romfs', args.exefs or extracted / 'exefs',
                                               everything=args.all, base_romfs=args.romfs_base)
    seed = out / 'seed-nvn2.bin'
    write_nvn2_seed(nvn2_stages, seed)
    runtime_dir = ROOT / 'build' / args.version
    shutil.copyfile(seed, runtime_dir / 'seed-nvn2.bin')
    jobs = write_shaders(shaders, out, args.all)
    a, b, c = (sum(1 for j in jobs.values() if j['origin'] == o) for o in ('bnsh', 'sharcb', 'main'))
    print(f'{len(shaders)} unique shaders found in all, {len(jobs)} selected; {rejected} .sharcb record candidates '
          f'without a control section rejected', flush=True)
    print(f'{len(nvn2_stages)} unique NVN 2 stages written to {seed} and runtime cache', flush=True)
    summary = f'shaders {len(jobs)} (bnsh {a}, sharcb {b}, main {c})'
    print(f'extracted {summary} in {time.monotonic() - t0:.1f} s', flush=True)
    if not args.translate:
        print(f'{summary}, total {time.monotonic() - t0:.1f} s')
        return 0
    if not XLATE.is_file() or not SPIRV_VAL.is_file():
        sys.exit(f'missing {XLATE} or {SPIRV_VAL}')
    ok, failed, bad = translate(jobs, out)
    print(f'{summary}, translated {ok}, failed {failed}, spirv-val errors {bad}, total {time.monotonic() - t0:.1f} s')
    return 0


if __name__ == '__main__':
    sys.exit(main())
