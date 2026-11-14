"""Draw runtime/spr.ico, the icon of SPR-Setup.exe and of the setup window for Splatoon Raiders.

    .venv\\Scripts\\python tools\\make_spr_icon.py

The icon is our own drawing, nothing taken from the game: the ink splat of the setup window's header (installer/gui.py
draw_splat, same outline and droplets, same THEMES['raiders'] colours) with a dark rim so it reads on light and dark
taskbars. Each size is rendered with 4x4 supersampling and stored as a PNG frame (16, 24, 32, 48, 64 and 256 px), which
Windows Vista and later read from .ico files. Standard library only.
"""
import math
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
from installer.gui import SPLAT_DROPS, THEMES  # noqa: E402

SIZES = (16, 24, 32, 48, 64, 256)
SS = 4   # supersampling per axis


def rgb(h):
    return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))


def blob_radius(a):
    """installer/gui.py splat_outline's radius factor at angle a."""
    return 1 + 0.16 * math.sin(5 * a + 0.4) + 0.07 * math.sin(9 * a + 1.3) + 0.04 * math.sin(13 * a)


def shapes(size):
    """(inside(x, y, grow) test, colour key) from back to front, in pixel coordinates of a size x size image."""
    # The whole splat, droplets included, spans about 2.9 blob radii each way; fit it with a small margin.
    r = size / 3.15
    cx = cy = size / 2
    out = [(lambda x, y, g: math.hypot(x - cx, y - cy) < r * blob_radius(math.atan2(y - cy, x - cx)) + g, 'accent')]
    for dx, dy, dr, key in SPLAT_DROPS:
        px, py, pr = cx + dx * r * 0.92, cy + dy * r * 0.92, dr * r * 1.15
        out.append((lambda x, y, g, px=px, py=py, pr=pr: math.hypot(x - px, y - py) < pr + g, key))
    hx, hy = cx - 0.35 * r, cy - 0.40 * r
    out.append((lambda x, y, g: ((x - hx) / (0.22 * r)) ** 2 + ((y - hy) / (0.14 * r)) ** 2 < 1, 'accent_hi'))
    return out


def render(size, t):
    rim = max(1.0, size / 24)   # the dark rim's width in pixels
    layers = shapes(size)
    colours = {k: rgb(t[k]) for k in ('accent', 'accent_hi', 'stripe')}
    dark = rgb(t['bg'])
    rows = []
    for py in range(size):
        row = bytearray([0])   # PNG filter type 0
        for px in range(size):
            acc = [0, 0, 0, 0]
            for sy in range(SS):
                for sx in range(SS):
                    x, y = px + (sx + 0.5) / SS, py + (sy + 0.5) / SS
                    colour = None
                    for inside, key in layers:
                        if inside(x, y, 0):
                            colour = colours[key]
                    if colour is None and any(inside(x, y, rim) for inside, key in layers if key != 'accent_hi'):
                        colour = dark
                    if colour is not None:
                        acc[0] += colour[0]; acc[1] += colour[1]; acc[2] += colour[2]; acc[3] += 255
            n = acc[3] // 255
            row += bytes((acc[0] // n, acc[1] // n, acc[2] // n, acc[3] // (SS * SS)) if n else (0, 0, 0, 0))
        rows.append(bytes(row))
    return png(size, b''.join(rows))


def png(size, raw):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


def ico(frames):
    head = struct.pack('<HHH', 0, 1, len(frames))
    offset = len(head) + 16 * len(frames)
    entries, data = b'', b''
    for size, blob in frames:
        dim = 0 if size >= 256 else size   # 0 means 256 in an ICONDIRENTRY
        entries += struct.pack('<BBBBHHII', dim, dim, 0, 0, 1, 32, len(blob), offset + len(data))
        data += blob
    return head + entries + data


def main():
    t = THEMES['raiders']
    out = ROOT / 'runtime' / 'spr.ico'
    out.write_bytes(ico([(s, render(s, t)) for s in SIZES]))
    print(f'{out}: {out.stat().st_size} bytes, sizes {", ".join(map(str, SIZES))}')


if __name__ == '__main__':
    main()
