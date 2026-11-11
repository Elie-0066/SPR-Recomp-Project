"""Report what the dump's ExeFS modules contain: NSO layout, module name, build id, symbols and import namespaces.

Needs no keys and writes nothing. It is the first thing to run on a new dump, so the lifter and the host work from
numbers rather than guesses.

Usage: raiders_survey.py --exefs <exefs folder> [--json]
"""
import argparse
import collections
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'recomp'))

from nso import Nso  # noqa: E402

ORDER = ['main'] + [f'subsdk{i}' for i in range(10)] + ['sdk']


def module_name(raw, path):
    """The NSO header ends with a NUL-terminated module name at ModuleNameOffset (0x100 here)."""
    off = struct.unpack_from('<I', raw, 0x1C)[0]
    if not 0 < off < len(raw):
        return ''
    end = raw.index(b'\0', off)
    return raw[off:end].decode('utf-8', 'replace')


def components(sym):
    """First Itanium-mangled components of a symbol, e.g. _ZN2nn2os13GetSystemTickEv -> ['nn', 'os', 'GetSystemTick']."""
    if not sym.startswith('_ZN'):
        return []
    out, i = [], 3
    while i < len(sym):
        j = i
        while j < len(sym) and sym[j].isdigit():
            j += 1
        if j == i:
            break
        n = int(sym[i:j])
        if n == 0 or j + n > len(sym):
            break
        out.append(sym[j:j + n])
        i = j + n
    return out


def namespace(sym):
    """Bucket of an import: nn::<subsystem>, std::, a C library name, or 'other'."""
    parts = components(sym)
    if parts:
        if parts[0] == 'nn' and len(parts) > 1:
            return 'nn::' + parts[1]
        if parts[0] == 'std':
            return 'std::'
        return 'c++::' + parts[0]
    return 'c-libc' if sym.islower() or '_' in sym else 'other'


def survey(path):
    n = Nso(str(path))
    raw = path.read_bytes()
    imports = [s[0] for s in n.imports()]
    buckets = collections.Counter(namespace(s) for s in imports)
    return {
        'module': module_name(raw, path),
        'build_id': n.build_id,
        'image': n.size,
        'text': n.text,
        'rodata': n.rodata,
        'data': n.data,
        'bss': n.bss_size,
        'needed': n.needed,
        'symbols': len(n.syms),
        'imports': len(imports),
        'exports': len(n.exports()),
        'import_namespaces': dict(buckets.most_common()),
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--exefs', type=Path, required=True)
    ap.add_argument('--json', action='store_true')
    args = ap.parse_args()
    out = []
    for name in ORDER:
        path = args.exefs / name
        if path.exists():
            out.append((name, survey(path)))
    if args.json:
        print(json.dumps(dict(out), indent=1))
        return
    for name, m in out:
        print(f'== {name}: module "{m["module"]}"  build {m["build_id"]}')
        print(f'   image 0x{m["image"]:x}  text 0x{m["text"][0]:x}+0x{m["text"][1]:x}  '
              f'ro 0x{m["rodata"][0]:x}+0x{m["rodata"][1]:x}  data 0x{m["data"][0]:x}+0x{m["data"][1]:x}  '
              f'bss 0x{m["bss"]:x}')
        print(f'   symbols {m["symbols"]}  imports {m["imports"]}  exports {m["exports"]}  needed {m["needed"]}')
        for ns, count in m['import_namespaces'].items():
            print(f'     {count:5} {ns}')


if __name__ == '__main__':
    main()
