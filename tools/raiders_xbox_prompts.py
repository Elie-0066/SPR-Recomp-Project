"""Build romfs-patches-xbox for Splatoon Raiders: the button prompts with the A and B, and the X and Y letters swapped.

pad_layout = xbox_prompts plays an Xbox pad by position, as nintendo does: the button that does Switch A (the right
one) is labelled B on an Xbox pad, so a prompt that names Switch A has to show B. runtime/romfs.c overlay_init loads
this folder in place of romfs-patches when that layout is chosen; the game reads these files once at boot, so the
prompts change at the next start.

Raiders draws its face-button prompts from two sets of pictures, and both are swapped:
- Layout textures KeyGuide_00^w A, KeyGuide_01^w B, KeyGuide_02^w X, KeyGuide_03^w Y (ASTC 4x4, 200x180): the key
  guides of the menus ("B Back" in the equipment menu, prompt layouts such as KeyDecideIconCmnA_00). They sit in the
  __Combined.bntx of 18 layout archives, some with one letter only. Every copy is checked against the reference
  picture (the first copy found) and then given its partner's picture: same format, size and tiling, so the bytes of
  one are a valid image of the other.
- The pictogram font font/SpPictogram.bffnt of every Font/Font*.Nin_Ounce_NVN2.bfarc.zs, for prompts inside text
  (U+E322 A, U+E323 B, U+E324 X, U+E325 Y: glyphs 21 to 24 of the first sheet). Its CMAP is only read to find the
  glyphs; the pictures move on the sheet (a 2D array BNTX, RGBA8 block-linear) together with their CWDH widths.
The pictures were identified by decoding them (update 65536). Names, glyph indices and identical copies are checked
before anything is written: another game version stops the tool instead of swapping the wrong pictures. Decompressed
files keep their sizes.

Input: the loose RomFS folder the game runs from (--romfs). Output: <out>/<path> for each rewritten archive, and
<out>/manifest.tsv (path, size, SHA-256 of the overlay, SHA-256 of the original) as runtime/romfs.c overlay_load
reads it. Run with .venv-py314 (compression.zstd).
"""
import argparse
import compression.zstd as zstd
import hashlib
import json
import re
import struct
from pathlib import Path

FONT = 'font/SpPictogram.bffnt'
# Code point -> glyph index in update 65536, and the swap: A <-> B, X <-> Y.
EXPECTED = {0xE322: 21, 0xE323: 22, 0xE324: 23, 0xE325: 24}
SWAPS = ((0xE322, 0xE323), (0xE324, 0xE325))
# The key guide textures: A <-> B, X <-> Y.
KEY_TEXTURES = {'KeyGuide_00^w': 'KeyGuide_01^w', 'KeyGuide_01^w': 'KeyGuide_00^w',
                'KeyGuide_02^w': 'KeyGuide_03^w', 'KeyGuide_03^w': 'KeyGuide_02^w'}


def sarc_members(data):
    """name -> (absolute start, end) of every SARC member."""
    assert data[:4] == b'SARC' and data[6:8] == b'\xff\xfe', 'not a little-endian SARC'
    header, size, start = struct.unpack_from('<HxxII', data, 4)
    assert size == len(data) and data[header:header + 4] == b'SFAT'
    count = struct.unpack_from('<H', data, header + 6)[0]
    names = header + 12 + count * 16
    assert data[names:names + 4] == b'SFNT'
    names += 8
    out = {}
    for i in range(count):
        _, attribute, lo, hi = struct.unpack_from('<4I', data, header + 12 + i * 16)
        offset = names + (attribute & 0xffffff) * 4
        out[data[offset:data.index(0, offset, start)].decode()] = (start + lo, start + hi)
    return out


def cmap_entries(data, base, end):
    """{code point: absolute offset of its u16 glyph index} for every CMAP section of the FFNT at data[base:end].
    Each CMAP: code range (u32, u32), mapping method (u16: 0 direct, 1 table, 2 scan), padding, next CMAP, then the
    method's data. Direct and table maps are kept as offsets too, so a code point mapped twice is refused."""
    assert data[base:base + 4] == b'FFNT' and data[base + 4:base + 6] == b'\xff\xfe', 'not a little-endian FFNT'
    pos, found = base + struct.unpack_from('<H', data, base + 6)[0], {}

    def add(code, at):
        assert code not in found, f'U+{code:04X} is in two CMAP sections'
        found[code] = at

    while pos + 8 <= end:
        magic, size = data[pos:pos + 4], struct.unpack_from('<I', data, pos + 4)[0]
        if magic == b'CMAP':
            first, last, method = struct.unpack_from('<IIH', data, pos + 8)
            body = pos + 0x18                                     # after the u32 offset of the next CMAP
            if method == 0:                                       # direct: index = offset + (code - first)
                start = struct.unpack_from('<H', data, body)[0]
                if first <= 0xE322 and 0xE325 <= last:
                    raise AssertionError(f'direct CMAP U+{first:04X}-U+{last:04X} (from glyph {start}) covers the '
                                         'button codes; this tool only swaps scan entries')
            elif method == 1:                                     # table: one u16 per code
                for k, code in enumerate(range(first, last + 1)):
                    add(code, body + 2 * k)
            elif method == 2:                                     # scan: count, then (u32 code, u16 index, pad)
                for k in range(struct.unpack_from('<I', data, body)[0]):
                    add(struct.unpack_from('<I', data, body + 4 + 8 * k)[0], body + 8 + 8 * k)
            else:
                raise AssertionError(f'CMAP mapping method {method}')
        if not size:
            break
        pos += size
    return found


def sections(data, base, end):
    """{magic: [absolute section start]} of the FFNT at data[base:end]."""
    pos, out = base + struct.unpack_from('<H', data, base + 6)[0], {}
    while pos + 8 <= end:
        magic, size = bytes(data[pos:pos + 4]), struct.unpack_from('<I', data, pos + 4)[0]
        out.setdefault(magic, []).append(pos)
        if not size:
            break
        pos += size
    return out


def gob_offset(x, y):
    """Byte offset of (x byte, y row) inside a 64 x 8 GOB in the Switch 2 layout (runtime/tex_deswizzle.c gob_off)."""
    return (x % 64 // 32) * 256 + (y % 8 // 4) * 128 + (x % 32 // 16) * 64 + (y % 4 // 2) * 32 + (y % 2) * 16 + x % 16


def swap_cells(data, base, end, pairs):
    """Swap the pictures of each (glyph, glyph) pair on the TGLP sheets of the FFNT at data[base:end], and their
    CWDH widths. The sheets are one BNTX (a 2D array texture, one layer per sheet, RGBA8 block-linear); the glyph
    cells are laid out row-major, cell size plus a 1-pixel border apart."""
    found = sections(data, base, end)
    tglp = found[b'TGLP'][0]
    cell_w, cell_h, sheets = data[tglp + 8], data[tglp + 9], data[tglp + 10]
    fmt, cols, rows, width, height, at = struct.unpack_from('<HHHHHI', data, tglp + 0x12)
    assert fmt == 14 and (cell_w, cell_h, cols, rows, width, height) == (200, 200, 5, 5, 1024, 1024), \
        (fmt, cell_w, cell_h, cols, rows, width, height)
    bntx = base + at
    assert data[bntx:bntx + 4] == b'BNTX'
    count, table = struct.unpack_from('<IQ', data, bntx + 0x24)
    assert count == 1
    info = bntx + struct.unpack_from('<Q', data, bntx + table)[0]
    assert data[info:info + 4] == b'BRTI'
    w, h, depth, layers, th = struct.unpack_from('<5I', data, info + 0x24)
    assert (w, h, depth, layers) == (width, height, 1, sheets), (w, h, depth, layers)
    image_size = struct.unpack_from('<I', data, info + 0x50)[0]
    pixels = bntx + struct.unpack_from('<Q', data, bntx + struct.unpack_from('<Q', data, info + 0x70)[0])[0]
    layer = image_size // layers
    row_bytes, bh = width * 4, 1 << th
    assert layer * layers == image_size and layer >= row_bytes * height

    def offset(x, y):   # byte offset of pixel (x, y) in one layer
        xb = x * 4
        return ((y // (8 * bh)) * (row_bytes // 64) * 512 * bh + (xb // 64) * 512 * bh +
                ((y % (8 * bh)) // 8) * 512 + gob_offset(xb, y))

    stride_x, stride_y = cell_w + 1, cell_h + 1
    for a, b in pairs:
        (sa, ca), (sb, cb) = divmod(a, cols * rows), divmod(b, cols * rows)
        xa, ya, xb, yb = ca % cols * stride_x, ca // cols * stride_y, cb % cols * stride_x, cb // cols * stride_y
        la, lb = pixels + sa * layer, pixels + sb * layer
        for dy in range(stride_y):
            for dx in range(stride_x):
                pa, pb = la + offset(xa + dx, ya + dy), lb + offset(xb + dx, yb + dy)
                data[pa:pa + 4], data[pb:pb + 4] = data[pb:pb + 4], data[pa:pa + 4]
    # CWDH: first glyph (u16), last glyph (u16), next CWDH (u32), then 3 bytes per glyph (left, glyph and advance).
    for cwdh in found[b'CWDH']:
        first, last = struct.unpack_from('<HH', data, cwdh + 8)
        for a, b in pairs:
            if first <= a <= last and first <= b <= last:
                pa, pb = cwdh + 0x10 + 3 * (a - first), cwdh + 0x10 + 3 * (b - first)
                data[pa:pa + 3], data[pb:pb + 3] = data[pb:pb + 3], data[pa:pa + 3]


def swap_prompts(archive):
    """The decompressed font archive with the pictures of the A/B and X/Y pictograms swapped. The CMAP stays: moving
    the pictures holds whether the game looks a pictogram up by code point or by glyph index."""
    data = bytearray(archive)
    lo, hi = sarc_members(data)[FONT]
    at = cmap_entries(data, lo, hi)
    glyphs = {code: struct.unpack_from('<H', data, at[code])[0] for code in EXPECTED if code in at}
    assert glyphs == EXPECTED, f'{FONT}: button glyphs {glyphs}, expected {EXPECTED} (another game version?)'
    swap_cells(data, lo, hi, [(glyphs[a], glyphs[b]) for a, b in SWAPS])
    return bytes(data)


def bntx_textures(data, base):
    """name -> (shape, absolute start, size) of every texture in the BNTX at data[base:]. shape: the BRTI fields from
    flags to layout (format, size, tiling, mips) and the image size, which two textures must share to trade bytes."""
    assert data[base:base + 4] == b'BNTX'
    count, table = struct.unpack_from('<IQ', data, base + 0x24)
    out = {}
    for k in range(count):
        info = base + struct.unpack_from('<Q', data, base + table + 8 * k)[0]
        assert data[info:info + 4] == b'BRTI'
        name = base + struct.unpack_from('<Q', data, info + 0x60)[0]
        name = bytes(data[name + 2:name + 2 + struct.unpack_from('<H', data, name)[0]]).decode()
        mips = base + struct.unpack_from('<Q', data, info + 0x70)[0]
        shape = bytes(data[info + 0x10:info + 0x3c]) + bytes(data[info + 0x50:info + 0x54])
        out[name] = (shape, base + struct.unpack_from('<Q', data, mips)[0],
                     struct.unpack_from('<I', data, info + 0x50)[0])
    return out


def swap_key_textures(archive, reference):
    """The decompressed layout archive with each KEY_TEXTURES picture replaced by its partner's, and the names
    replaced. reference (name -> (shape, picture)) keeps the first copy of each picture; every copy must equal it."""
    data, changed = bytearray(archive), []
    for member, (lo, hi) in sarc_members(data).items():
        if not member.endswith('.bntx'):
            continue
        textures = bntx_textures(data, lo)
        for name in KEY_TEXTURES:
            if name in textures:
                shape, at, size = textures[name]
                picture = bytes(data[at:at + size])
                assert reference.setdefault(name, (shape, picture)) == (shape, picture), f'{member}: {name} differs'
        for name, partner in KEY_TEXTURES.items():
            if name in textures:
                shape, at, size = textures[name]
                if partner not in reference:
                    continue   # the collecting pass (main) sees the partner later
                assert reference[partner][0] == shape, f'{name} and {partner} differ in format, size or tiling'
                data[at:at + size] = reference[partner][1]
                changed.append(name)
    return bytes(data), changed


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--romfs', type=Path, required=True, help='the loose RomFS folder the game runs from')
    ap.add_argument('--out', type=Path, required=True, help='the romfs-patches-xbox folder to write')
    ap.add_argument('--level', type=int, default=12, help='zstd level of the rewritten archives (default 12)')
    args = ap.parse_args()

    def read(path):
        raw = (args.romfs / path).read_bytes()
        info = zstd.get_frame_info(raw)
        # Raiders compresses these archives without a dictionary (checked on update 65536) and ships no ZsDic pack.
        assert not info.dictionary_id, f'{path}: compressed with zstd dictionary {info.dictionary_id}'
        return raw, zstd.decompress(raw)

    outputs = {}   # RomFS path -> (original compressed bytes, new contents)
    fonts = sorted(p.relative_to(args.romfs).as_posix() for p in (args.romfs / 'Font').glob('*.bfarc.zs')
                   if re.fullmatch(r'Font(_\w+)?\.Nin_Ounce_NVN2\.bfarc\.zs', p.name))
    assert fonts, f'no Font/Font*.Nin_Ounce_NVN2.bfarc.zs in {args.romfs}'
    for path in fonts:
        raw, archive = read(path)
        outputs[path] = (raw, swap_prompts(archive))

    # Two passes over the layouts: the first collects a reference copy of every picture (an archive may carry one
    # letter only), the second swaps them all.
    layouts = sorted(p.relative_to(args.romfs).as_posix() for p in args.romfs.rglob('*.blarc.zs'))
    reference, carrying = {}, []
    for path in layouts:
        _, archive = read(path)
        if any(name.encode() in archive for name in KEY_TEXTURES):
            swap_key_textures(archive, reference)
            carrying.append(path)
    assert set(reference) == set(KEY_TEXTURES), f'key guide pictures found: {sorted(reference)}'
    textures = {}
    for path in carrying:
        raw, archive = read(path)
        patched, changed = swap_key_textures(archive, reference)
        if changed:
            outputs[path] = (raw, patched)
            textures[path] = changed

    lines = []
    for path, (raw, contents) in outputs.items():
        data = zstd.compress(contents, level=args.level)
        assert zstd.decompress(data) == contents and len(contents) == len(zstd.decompress(raw))
        target = args.out / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        lines.append('\t'.join([path, str(len(data)), hashlib.sha256(data).hexdigest(), hashlib.sha256(raw).hexdigest()]))
    (args.out / 'manifest.tsv').write_text('# path\tsize\tsha256\toriginal_sha256 (tools/raiders_xbox_prompts.py)\n'
                                           + ''.join(line + '\n' for line in lines), encoding='utf-8', newline='\n')
    print(json.dumps(dict(status='pass', font=FONT, fonts=fonts, swapped=[f'U+{a:04X}<->U+{b:04X}' for a, b in SWAPS],
                          layouts=len(textures), textures=sum(len(v) for v in textures.values()))))


if __name__ == '__main__':
    main()
