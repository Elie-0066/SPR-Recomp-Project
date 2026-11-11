"""Pack a dumped RomFS directory into romfs.bin and index.json, the pair tools/romfs_index.py and runtime/romfs.c read.

The TotK and DKB paths get `romfs.bin` from hactool, because their RomFS arrives inside the game's NCA. A dump that is
already a directory tree (an emulator's, or a hactool -x run) has the same files but no image, and the runtime reads a
packed image with an index next to it. This writes both, in the format tools/extract.py produces:
`{status, romfs_size, romfs_sha256, files: [{path, offset, size}], integrity}`, with paths relative to the RomFS root
and offsets into the image.

`--update` overlays a dumped update directory on top of the base one, the merge hactool performs with --basenca: a path
the update has is the update's file, everything else comes from the base. Only the files are merged, so a RomFS that
the update changes elsewhere (its table of contents) is not reproduced; the game reads files by path, not by table.

    .venv-py314\\Scripts\\python tools\\pack_romfs.py --romfs "<dump>\\base\\romfs" \\
        --update "<dump>\\updates\\65536\\romfs" --out build\\extracted\\raiders\\romfs

Nothing derived from the game enters the repository: --out is under build/.
"""
import argparse
import hashlib
import json
import os
import sys
import time
from pathlib import Path

CHUNK = 1 << 20

def files_of(directory):
    """{relative path with forward slashes: file} for every file below directory, in path order."""
    out = {}
    for dirpath, dirnames, filenames in os.walk(directory):
        dirnames.sort()
        for name in sorted(filenames):
            path = Path(dirpath) / name
            rel = path.relative_to(directory).as_posix()
            if rel in out:
                sys.exit(f'pack_romfs: {rel} appears twice below {directory}')
            out[rel] = path
    return out


def pack(files, image):
    """Append each file to image and return the index rows, hashing the image as it is written."""
    digest = hashlib.sha256()
    rows, offset, size, done, t0 = [], 0, 0, 0, time.monotonic()
    for rel in sorted(files, key=lambda p: p.encode()):
        src = files[rel]
        n = 0
        with src.open('rb') as f:
            while data := f.read(CHUNK):
                image.write(data)
                digest.update(data)
                n += len(data)
        rows.append({'path': rel, 'offset': offset, 'size': n})
        offset += n
        size += n
        done += 1
        if time.monotonic() - t0 > 10:
            print(f'pack_romfs: {done}/{len(files)} files, {size / (1 << 30):.1f} GiB', file=sys.stderr, flush=True)
            t0 = time.monotonic()
    return rows, size, digest.hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--romfs', type=Path, required=True, help='the dumped base RomFS directory')
    ap.add_argument('--update', type=Path, action='append',
                    help='a dumped update RomFS directory whose files replace the base ones (repeatable)')
    ap.add_argument('--out', type=Path, required=True, help='the directory to write romfs.bin and index.json into')
    ap.add_argument('--force', action='store_true', help='replace an existing romfs.bin')
    args = ap.parse_args()

    base, updates = args.romfs.resolve(), [p.resolve() for p in args.update or []]
    for d in [base, *updates]:
        if not d.is_dir():
            sys.exit(f'pack_romfs: {d} is not a directory')
    out = args.out.resolve()
    image_path, index_path = out / 'romfs.bin', out / 'index.json'
    if image_path.exists() and not args.force:
        sys.exit(f'pack_romfs: {image_path} exists; pass --force to replace it')

    files = files_of(base)
    print(f'pack_romfs: {len(files)} files in {base}', file=sys.stderr, flush=True)
    for d in updates:
        extra = files_of(d)
        replaced, added = 0, 0
        for rel, path in extra.items():
            replaced += rel in files
            added += rel not in files
            files[rel] = path   # the update wins
        print(f'pack_romfs: {d}: {replaced} files replaced, {added} added, {len(files)} in all', file=sys.stderr,
              flush=True)
    folded = {}
    for rel in files:
        if folded.setdefault(rel.lower(), rel) != rel:
            print(f'pack_romfs: warning: {rel} and {folded[rel.lower()]} differ only in case', file=sys.stderr)

    out.mkdir(parents=True, exist_ok=True)
    tmp = image_path.with_suffix('.bin.part')
    with tmp.open('wb') as image:
        rows, size, sha = pack(files, image)
    os.replace(tmp, image_path)
    index = {'status': 'pass', 'romfs_size': size, 'romfs_sha256': sha, 'files': rows,
             'integrity': 'packed from a dumped directory tree; no IVFC levels to verify'}
    index_path.write_text(json.dumps(index, indent=2) + '\n', encoding='utf-8')
    print(f'pack_romfs: {image_path} {size / (1 << 30):.2f} GiB, {len(rows)} files, sha256 {sha[:16]}...')
    print(f'pack_romfs: {index_path}')


if __name__ == '__main__':
    main()
