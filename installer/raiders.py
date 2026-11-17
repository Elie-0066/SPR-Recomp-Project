"""Validated SPR Recomp (Splatoon Raiders) installs from a .nup container or ExeFS and RomFS folders. No keys.

The flow is Bananza's (installer/bananza.py), whose path checks, locks, translator step, host check and tool runner it
reuses. What differs for this game:

- The ExeFS has three modules, main, sdk and subsdk0, lifted whole and without patch sets. Update 65536
  (tools/data/expected-raiders.json) is the version the project is developed and tested on. The other versions of
  tools/data/expected-raiders-*.json, and any other version whose main.npdm (or .nup package) carries the game's
  program id, are built too, with the warning warn.spr_version_experimental: nothing says the lifter or the runtime
  handles them. The executable is pinned to the ExeFS it was built from either way.
- The runtime reads the RomFS as a loose folder (tools/romfs_index.py indexes it in place), so nothing is packed. A
  RomFS folder is used where it is and recorded in the executable; a .nup is extracted loose into <dest>/romfs, the
  base RomFS with the update's laid over it as Eglantine runs it (tools/extract_nup.py --select latest).
- The shader census is tools/shader_census.py, which reads the loose RomFS and the ExeFS.
- The executable is spr-recomp.exe and the install is recorded in spr-install.json.

The step ids are Bananza's, so the window's step list (installer/guilogic.py DKB_STEP_ORDER) serves both.
"""
import copy
import hashlib
import json
import os
import shutil
import signal
import struct
import tempfile
import time
from dataclasses import replace
from pathlib import Path

from . import bananza as B
from . import ini
from . import layout as layouts
from . import state as statefile
from .events import Reporter
from .runner import Context, InstallError, Step, run_steps

GAME = 'raiders'
STATE = 'spr-install.json'
OWNER = '.spr-setup-owner.json'
DEST_OWNER = '.spr-install-owner.json'
EXE_NAME = 'spr-recomp' + B.EXE
MODULES = ('main', 'sdk', 'subsdk0')
EXEFS_FILES = ('main', 'main.npdm', 'rtld', 'sdk', 'subsdk0')
NUP_PART = '.setup-nup'   # the .nup is extracted here, inside the install folder, then renamed into place
XBOX_DIR = 'romfs-patches-xbox'   # the Xbox button prompts (Prepare.xbox_prompts)
XBOX_TOOL = 'raiders_xbox_prompts.py'
# Work space beyond the RomFS: about 1.3 GB of generated C, 0.4 GB of objects, the 300 MB executable, the shader and
# pipeline caches.
TEMP_NEED = 6 << 30
DEST_NEED = 3 << 30


def msys2_layout(layout, options):
    """A checkout without a tool package or an llvm-mingw tree builds with MSYS2 UCRT64, as docs/building.md sets a
    development machine up: its clang, ninja and the SDL3, Opus and libiconv DLLs, and tools/build_game.py's own MSYS2
    defaults for the headers and libraries (no --toolchain or --deps). A release always carries its own tools."""
    ucrt = layouts.UCRT
    if (os.name != 'nt' or not layout.dev or layout.clang.is_file() or options.package or options.toolchain or
            not (ucrt / 'clang.exe').is_file()):
        return layout
    dlls = layout.dlls if options.dll_dir else tuple(
        ucrt / n for n in ('SDL3.dll', 'libopus-0.dll', 'libiconv-2.dll') if (ucrt / n).is_file())
    ninja = layout.ninja if layout.ninja.is_file() else ucrt / 'ninja.exe'
    return replace(layout, llvm=ucrt.parent, deps=None, dlls=dlls, ninja=ninja)


def uses_msys2(layout):
    return os.name == 'nt' and layout.llvm.resolve() == layouts.UCRT.parent.resolve()


def known_versions(layout):
    """The records of tools/data/expected-raiders.json (the tested update) and expected-raiders-*.json."""
    return [json.loads(p.read_text(encoding='utf-8'))
            for p in sorted((layout.tools / 'data').glob(f'expected-{GAME}*.json'))]


def program_id(layout):
    """The game's program id, from the tested version's record."""
    return int(json.loads((layout.tools / 'data' / f'expected-{GAME}.json').read_text(encoding='utf-8'))['title_id'], 16)


def npdm_program_id(data):
    """The program id of a main.npdm: META header, ACI0 offset and size at 0x70, the id at ACI0+0x10. None if malformed."""
    if len(data) < 0x78 or data[:4] != b'META':
        return None
    offset, size = struct.unpack_from('<II', data, 0x70)
    if size < 0x18 or offset + 0x18 > len(data) or data[offset:offset + 4] != b'ACI0':
        return None
    return struct.unpack_from('<Q', data, offset + 0x10)[0]


def identify(layout, hashes, title_version=None, display=None):
    """The version record whose five ExeFS hashes all match, else one made from the hashes: an unknown version of the
    game. 'verified' is true for the tested update only; build_game.py reads display_version for the settings menu."""
    for record in known_versions(layout):
        if record['exefs_sha256'] == {name: hashes.get(name) for name in record['exefs_sha256']}:
            return {k: v for k, v in record.items() if k != '_comment'}
    if display is None:
        display = f'title version {title_version}' if title_version is not None else 'unknown version'
    return {'title_id': f'{program_id(layout):016x}', 'display_version': display, 'title_version': title_version,
            'verified': False, 'exefs_sha256': dict(hashes)}


def exefs_folder_hashes(exefs):
    """The SHA-256 of the five ExeFS files of a folder."""
    hashes = {}
    for name in EXEFS_FILES:
        p = exefs / name
        B.ensure_plain(p)
        if not p.is_file():
            raise InstallError('spr.exefs_invalid', path=str(p))
        hashes[name] = B.sha256(p)
    return hashes


def check_exefs(exefs, hashes):
    """Every ExeFS file has the SHA-256 preflight recorded."""
    for name, value in hashes.items():
        p = exefs / name
        B.ensure_plain(p)
        if not p.is_file() or B.sha256(p) != value:
            raise InstallError('spr.exefs_invalid', path=str(p))


def romfs_listing(root):
    """(path, size) of every file under a loose RomFS folder, without reading the files: 46,000 files of game data are
    not hashed on every run. The runtime's index (romfs_index.py) is rebuilt from the same folder at compile time."""
    files = B.tree_files(root)
    if not files:
        raise InstallError('dkb.romfs_empty', path=str(root))
    return [(p.relative_to(root).as_posix(), p.stat().st_size) for p in files]


def romfs_ok(root):
    """A loose RomFS of this game: its shader packages are where the census reads them."""
    return (root / 'Shader').is_dir()


def nup_source(layout, path, select):
    """The version record, the RomFS listing and its size of a .nup, from its table of contents alone (nothing is
    decompressed). select is extract_nup.py's --select: latest (the newest update over the base) or base."""
    tool = B.tool_module(layout, 'extract_nup.py')
    try:
        container = tool.Nup(path)
    except (tool.NupError, OSError, ValueError, UnicodeDecodeError) as e:
        raise InstallError('dkb.nup_invalid', path=str(path), detail=str(e)) from None
    try:
        plan = tool.install_plan(container, select)
        exefs = {'/'.join(parts): idx for parts, idx in tool.merged_entries(container, [plan['exefs']], 'exefs')}
        romfs = tool.merged_entries(container, plan['romfs'], 'romfs')
        package = plan['exefs']
        if package.title_id & ~0xFFF != program_id(layout):   # an update's id is the base's plus 0x800
            raise InstallError('spr.wrong_game', path=str(path))
        missing = [name for name in EXEFS_FILES if name not in exefs]
        if missing:
            raise InstallError('spr.exefs_invalid', path=f'{path} (exefs/{missing[0]})')
        hashes = {name: container.files[exefs[name]][4].hex() for name in EXEFS_FILES}
        version = identify(layout, hashes, package.version, package.display_version or None)
        if not romfs:
            raise InstallError('dkb.romfs_empty', path=f'{path} (romfs)')
        size = sum(container.files[idx][1] for _, idx in romfs)
        listing = [('/'.join(parts), container.files[idx][1]) for parts, idx in romfs]
        compressed = any(container.files[i][5] for i in (*exefs.values(), *(idx for _, idx in romfs)))
    except tool.NupError as e:
        raise InstallError('dkb.nup_invalid', path=str(path), detail=str(e)) from None
    finally:
        container.close()
    if compressed:
        probe = B.subprocess.run([str(layout.python), str(layout.tools / 'extract_nup.py'), '--check-zstd'],
                                 capture_output=True, text=True)
        if probe.returncode:
            raise InstallError('dkb.nup_zstd', path=str(path))
    return version, listing, size


def plain_destination(dest, dlls=()):
    files = (STATE, STATE + '.tmp', DEST_OWNER, '.dkb-setup.lock', 'dkb.ini', 'dkb.ini.tmp', 'totk.ini',
             'totk.ini.tmp', 'romfs-index.bin', 'seed-nvn2.bin', EXE_NAME, 'totk_xlate' + B.EXE, 'totk_xlate.id',
             *(p.name for p in dlls))
    for name in ('exefs', 'romfs', NUP_PART, XBOX_DIR, *files, *(name + '.setup-part' for name in files)):
        B.plain_tree(dest / name)


def plain_outputs(ctx):
    B.plain_tree(ctx.temp)
    plain_destination(ctx.dest, ctx.layout.dlls)


def own_data(dest, old, options):
    """True when this run rebuilds from the ExeFS and RomFS an earlier .nup install left in the install folder: the
    folder records a .nup install and holds both, and either no game files are given while the recorded .nup is gone,
    or the folders given are those two. Without this an update asked for the .nup again, and choosing the install's own
    folders failed because they sit inside the output folder (SPR issue #17)."""
    recorded = (old or {}).get('sources') or {}
    if not recorded.get('nup') and not recorded.get('installed'):
        return False
    exefs, romfs = dest / 'exefs', dest / 'romfs'
    if not (exefs / 'main').is_file() or not romfs_ok(romfs):
        return False
    if getattr(options, 'nup', None):
        return False
    if not options.exefs and not options.romfs:
        return not recorded.get('nup') or not Path(recorded['nup']).is_file()
    same = lambda given, own: given is not None and Path(given).resolve() == own.resolve()
    return same(options.exefs, exefs) and same(options.romfs, romfs)


def preflight(layout, options, dest, temp, rep):
    if options.base or options.update or options.keys:
        raise InstallError('spr.update_unsupported')
    if options.jobs < 1:
        raise InstallError('dkb.jobs')
    if options.lang and ini.canonical_language(options.lang) not in ini.LANGUAGES:
        raise InstallError('ui.need_lang')
    B.ensure_plain(dest / STATE)
    old = statefile.load(dest, STATE)
    if old is not None and (old.get('game') != GAME or old.get('destination') != str(dest)):
        raise InstallError('spr.dest_unrelated', path=str(dest))
    nup_arg = getattr(options, 'nup', None)
    if nup_arg and (options.exefs or options.romfs):
        raise InstallError('dkb.nup_and_folders')
    recorded = old['sources'] if old else {}
    own = own_data(dest, old, options)
    if not own and not nup_arg and not options.exefs and not options.romfs and recorded.get('nup'):
        nup_arg = Path(recorded['nup'])
    rep.progress('check', 'dkb_base')
    if own:
        # The install's own ExeFS and RomFS, which an earlier .nup install extracted there: an update to a new release
        # rebuilds from them, without the .nup (SPR issue #17). The .nup's path stays recorded for a later run.
        exefs, romfs = dest / 'exefs', dest / 'romfs'
        hashes = exefs_folder_hashes(exefs)
        try:
            game = npdm_program_id((exefs / 'main.npdm').read_bytes())
        except OSError:
            game = None
        if game != program_id(layout):
            raise InstallError('spr.wrong_game', path=str(exefs / 'main.npdm'))
        version = identify(layout, hashes)
        listing, romfs_size = romfs_listing(romfs), 0
        inputs = ()
        sources = {k: v for k, v in recorded.items() if k in ('nup', 'nup_package')}
        sources['installed'] = True
        romfs_dir = romfs
    elif nup_arg:
        for p in (nup_arg, dest, temp):
            B.ensure_plain(p.absolute())
        nup = nup_arg.resolve()
        if not nup.is_file():
            if not getattr(options, 'nup', None) and recorded.get('nup'):   # the path came from the earlier install
                raise InstallError('dkb.nup_moved', path=str(nup))
            raise InstallError('dkb.need_nup')
        select = getattr(options, 'nup_package', None) or recorded.get('nup_package') or 'latest'
        version, listing, romfs_size = nup_source(layout, nup, select)
        inputs, sources = (nup,), {'nup': str(nup), 'nup_package': select}
        romfs_dir = dest / 'romfs'
    else:
        exefs_arg = options.exefs or (Path(recorded['exefs']) if recorded.get('exefs') else None)
        romfs_arg = options.romfs or (Path(recorded['romfs']) if recorded.get('romfs') else None)
        if not exefs_arg or not romfs_arg:
            raise InstallError('spr.need_folders')
        for p in (exefs_arg, romfs_arg, dest, temp):
            B.ensure_plain(p.absolute())
        exefs, romfs = exefs_arg.resolve(), romfs_arg.resolve()
        if not exefs.is_dir() or not romfs.is_dir():
            raise InstallError('spr.need_folders')
        if not romfs_ok(romfs):
            raise InstallError('spr.romfs_invalid', path=str(romfs))
        hashes = exefs_folder_hashes(exefs)
        try:
            game = npdm_program_id((exefs / 'main.npdm').read_bytes())
        except OSError:
            game = None
        if game != program_id(layout):
            raise InstallError('spr.wrong_game', path=str(exefs / 'main.npdm'))
        version = identify(layout, hashes)
        listing, romfs_size = romfs_listing(romfs), 0   # read in place: no copy on the install disk
        inputs, sources = (exefs, romfs), {'exefs': str(exefs), 'romfs': str(romfs)}
        romfs_dir = romfs
    if dest.exists() and not dest.is_dir():
        raise InstallError('spr.dest_unrelated', path=str(dest))
    for a, b in ((dest, temp), *((out, source) for out in (dest, temp) for source in inputs)):
        if B.contains(a, b) or B.contains(b, a):
            raise InstallError('dkb.path_overlap', path=str(a), other=str(b))
    if dest.exists() and old is None and any(dest.iterdir()):
        try:
            B.ensure_plain(dest / DEST_OWNER)
            partial = json.loads((dest / DEST_OWNER).read_text(encoding='utf-8'))
            ours = partial == {'game': GAME, 'destination': str(dest)}
            ours &= {p.name for p in dest.iterdir()} <= {DEST_OWNER, '.dkb-setup.lock'}
        except (OSError, ValueError):
            ours = False
        if not ours:
            raise InstallError('spr.dest_unrelated', path=str(dest))
    if B.nuget_needs_fallback(temp) and any(c.isspace() for c in tempfile.gettempdir()):
        raise InstallError('dkb.temp_space', path=str(temp))
    owner = {'game': GAME, 'destination': str(dest), 'temporary': str(temp)}
    for p in (temp / OWNER, dest / STATE, dest / (STATE + '.tmp'), dest / DEST_OWNER):
        B.ensure_plain(p)
    if temp.exists() and any(temp.iterdir()):
        try:
            actual = json.loads((temp / OWNER).read_text(encoding='utf-8'))
        except (ValueError, OSError):
            actual = None
        if actual != owner:
            raise InstallError('dkb.temp_unrelated', path=str(temp))
    B.plain_tree(temp)
    plain_destination(dest, layout.dlls)
    if not version.get('verified'):
        rep.warning('warn.spr_version_experimental', version=version['display_version'])
    missing = [p for p in (layout.clang, layout.ninja, layout.recomp / 'gen.py', layout.tools / 'shader_census.py',
                           layout.tools / 'romfs_index.py', layout.tools / 'build_game.py', layout.tools / XBOX_TOOL,
                           *([layout.tools / 'extract_nup.py'] if nup_arg else []),
                           layout.runtime / 'nvn2_lower.cpp', layout.tools / 'data' / f'expected-{GAME}.json')
               if not p.is_file()]
    if layout.deps:
        missing += [p for p in (layout.deps / 'include', layout.deps / 'lib') if not p.is_dir()]
    if os.name == 'nt':
        names = {p.name for p in layout.dlls}
        if not {'SDL3.dll', 'libopus-0.dll'} <= names:
            raise InstallError('dkb.dlls_missing')
    if missing:
        raise InstallError('dkb.tools_missing', paths='; '.join(map(str, missing)))
    identity = B.translator_identity(layout)
    candidates = [options.translator] if options.translator else [layout.xlate, dest / ('totk_xlate' + B.EXE)]
    translator = None
    for candidate in candidates:
        if candidate.is_file() and candidate.with_suffix('.id').is_file():
            if candidate.with_suffix('.id').read_text(encoding='ascii').strip() == identity:
                translator = candidate.resolve()
                break
    if options.translator and translator is None:
        raise InstallError('dkb.translator_mismatch')
    dotnet = options.dotnet or layout.root / 'toolchain' / 'dotnet' / ('dotnet' + B.EXE)
    if not dotnet.is_file():
        dotnet = Path(shutil.which('dotnet') or dotnet)
    if not translator:
        proc = B.subprocess.run([str(dotnet), '--list-sdks'], capture_output=True, text=True) if dotnet.is_file() else None
        if proc is None or proc.returncode or not any(line.startswith('10.') for line in proc.stdout.splitlines()):
            raise InstallError('dkb.dotnet_missing')
    # Check (B.Check) sizes the space from romfs_size: a .nup's RomFS is written to the install folder, a folder's is not.
    return dict(old=old, game=GAME, sources=sources, version=version, hashes=version['exefs_sha256'], romfs_id=B.digest(listing), romfs_size=romfs_size,
                romfs_files=len(listing), romfs_dir=str(romfs_dir), owner=owner, translator=translator,
                identity=identity, dotnet=dotnet,
                pipeline=B.digest(*(B.sha256(Path(__file__).with_name(n))
                                    for n in ('raiders.py', 'bananza.py', 'runner.py', 'state.py'))))


class RaidersContext(Context):
    def save(self):
        plain_outputs(self)
        B.ensure_plain(self.dest / STATE)
        B.ensure_plain(self.dest / (STATE + '.tmp'))
        self.dest.mkdir(parents=True, exist_ok=True)
        statefile.save(self.dest, self.state, STATE)


def owned_dir(ctx, path):
    """A work folder in the temp folder, emptied: B.owned_dir with this installer's owner record."""
    B.ensure_plain(path)
    if not B.contains(ctx.temp, path) or path == ctx.temp:
        raise InstallError('dkb.path_overlap', path=str(path), other=str(ctx.temp))
    if json.loads((ctx.temp / OWNER).read_text(encoding='utf-8')) != ctx.info['owner']:
        raise InstallError('dkb.temp_unrelated', path=str(ctx.temp))
    if path.exists():
        B.plain_tree(path)
        shutil.rmtree(path)
    path.mkdir(parents=True)


def run_tool(ctx, step, command, env=None, cwd=None, name=None):
    """B.run_tool, which checks Bananza's file names in the install folder; this game's are checked here too."""
    plain_outputs(ctx)
    B.run_tool(ctx, step, command, env, cwd, name)
    plain_outputs(ctx)


class Check(B.Check):
    def run(self, ctx):
        disk = lambda path: next(p for p in (path, *path.parents) if p.exists())
        dest_disk, temp_disk = disk(ctx.dest), disk(ctx.temp)
        free, temp_free = shutil.disk_usage(dest_disk).free, shutil.disk_usage(temp_disk).free
        same_disk = dest_disk.stat().st_dev == temp_disk.stat().st_dev
        need = ctx.info['romfs_size'] + DEST_NEED + (TEMP_NEED if same_disk else 0)
        ctx.reporter.emit('space', dest=str(ctx.dest), need=need, free=free, temp=str(ctx.temp),
                          temp_need=TEMP_NEED, temp_free=temp_free, same_disk=same_disk)
        if free < need or temp_free < TEMP_NEED:
            raise InstallError('dkb.space', need=round(need / (1 << 30), 1))
        ctx.temp.mkdir(parents=True, exist_ok=True)
        self.host(ctx)
        ctx.verify_kept(self.id)


class Data(Step):
    """The ExeFS into <dest>/exefs; for a .nup also the loose RomFS into <dest>/romfs."""
    id = 'dkb_data'

    def key(self, ctx):
        parts = [ctx.info['hashes'], ctx.info['romfs_id'], ctx.info['pipeline'], ctx.info['version']['display_version']]
        if 'nup' in ctx.info['sources']:
            parts.append(B.sha256(ctx.layout.tools / 'extract_nup.py'))
        return B.digest(*parts)

    def current(self, ctx, record):
        try:
            check_exefs(ctx.dest / 'exefs', ctx.info['hashes'])
            romfs = Path(ctx.info['romfs_dir'])
            return romfs_ok(romfs) and record.get('romfs_files') == ctx.info['romfs_files']
        except (OSError, InstallError):
            return False

    def run(self, ctx):
        sources = ctx.info['sources']
        if sources.get('installed'):   # the install folder's own ExeFS and RomFS (own_data): nothing to copy
            check_exefs(ctx.dest / 'exefs', ctx.info['hashes'])
            if B.digest(romfs_listing(ctx.dest / 'romfs')) != ctx.info['romfs_id']:
                raise InstallError('dkb.input_changed', path=str(ctx.dest / 'romfs'))
            return {'romfs_files': ctx.info['romfs_files']}
        if 'nup' in sources:
            # Extracted inside the install folder, so the multi-gigabyte RomFS is renamed into place, not copied; every
            # file is checked against its SHA-256 record in the container.
            part = ctx.dest / NUP_PART
            B.ensure_plain(part)
            if part.exists():
                B.plain_tree(part)
                shutil.rmtree(part)
            run_tool(ctx, self.id, [ctx.layout.python, ctx.layout.tools / 'extract_nup.py', sources['nup'],
                                    '--select', sources['nup_package'], '--out', part])
            check_exefs(part / 'exefs', ctx.info['hashes'])
            listing = romfs_listing(part / 'romfs')
            if B.digest(listing) != ctx.info['romfs_id']:
                raise InstallError('dkb.input_changed', path=sources['nup'])
            romfs = ctx.dest / 'romfs'
            B.ensure_plain(romfs)
            if romfs.exists():
                B.plain_tree(romfs)
                shutil.rmtree(romfs)
            os.replace(part / 'romfs', romfs)
            exefs = part / 'exefs'
        else:
            exefs = Path(sources['exefs'])
            romfs = Path(sources['romfs'])
            if B.digest(romfs_listing(romfs)) != ctx.info['romfs_id']:
                raise InstallError('dkb.input_changed', path=str(romfs))
        for name in ctx.info['hashes']:
            B.copy_file(exefs / name, ctx.dest / 'exefs' / name)
        check_exefs(ctx.dest / 'exefs', ctx.info['hashes'])
        if 'nup' in sources:
            shutil.rmtree(ctx.dest / NUP_PART)
        return {'romfs_files': ctx.info['romfs_files']}


class Lift(Step):
    id, transient = 'lift', True

    def key(self, ctx):
        return B.digest(ctx.keys['dkb_data'], B.source_key(ctx.layout, 'recomp'))

    def current(self, ctx, record):
        gen = ctx.temp / 'gen'
        try:
            for name in MODULES:
                m = json.loads((gen / name / 'manifest.json').read_text(encoding='utf-8'))
                if not m['chunks'] or not all((gen / name / chunk).is_file() for chunk in m['chunks']):
                    return False
            return (gen / 'layout.json').is_file() and record.get('outputs') == B.tree_key(gen)
        except (OSError, KeyError, ValueError):
            return False

    def run(self, ctx):
        gen = ctx.temp / 'gen'
        plain_outputs(ctx)
        gen.mkdir(parents=True, exist_ok=True)
        run_tool(ctx, self.id, [ctx.layout.python, ctx.layout.recomp / 'gen.py', '--exefs', ctx.dest / 'exefs',
                                '--out', gen, '--jobs', ctx.options.jobs])
        return {'outputs': B.tree_key(gen)}


class Compile(Step):
    id = 'compile'

    def key(self, ctx):
        return B.digest(ctx.keys['lift'], B.source_key(ctx.layout, 'runtime', 'tools/build_game.py', 'tools/romfs_index.py'),
                        ctx.info['romfs_dir'], str(ctx.layout.llvm), str(ctx.layout.deps),
                        B.sha256(ctx.layout.clang), B.sha256(ctx.layout.ninja),
                        B.tree_key(ctx.layout.deps) if ctx.layout.deps else 'system-libraries',
                        [(str(p), B.sha256(p)) for p in ctx.layout.dlls])

    def current(self, ctx, record):
        exe = ctx.dest / EXE_NAME
        try:
            return (exe.is_file() and B.sha256(exe) == record.get('sha256') and
                    B.sha256(ctx.dest / 'romfs-index.bin') == record.get('index_sha256') and
                    all(B.sha256(ctx.dest / p.name) == B.sha256(p) for p in ctx.layout.dlls))
        except OSError:
            return False

    def run(self, ctx):
        out = ctx.temp / 'build'
        # The version record preflight identified: build_game.py pins the executable to its hashes and shows its
        # display_version. Written to the temp folder, since an unknown version has no file under tools/data.
        expected = ctx.temp / f'expected-{GAME}.json'
        B.ensure_plain(expected)
        expected.write_text(json.dumps(ctx.info['version'], indent=2) + '\n', encoding='utf-8')
        args = [ctx.layout.python, ctx.layout.tools / 'build_game.py', '--version', GAME, '--expected', expected, '--gen', ctx.temp / 'gen', '--out', out,
                '--exefs', ctx.dest / 'exefs', '--romfs', ctx.info['romfs_dir'], '--profile', ctx.dest / 'profile',
                '--runtime', ctx.layout.runtime, '--ninja', ctx.layout.ninja, '--jobs', ctx.options.jobs, '--full-build']
        if not uses_msys2(ctx.layout):   # MSYS2 UCRT64 is build_game.py's default toolchain (msys2_layout)
            args += ['--toolchain', ctx.layout.llvm]
        if ctx.layout.deps:
            args += ['--deps', ctx.layout.deps]
        if ctx.layout.sysroot:
            args += ['--sysroot', ctx.layout.sysroot]
        run_tool(ctx, self.id, args)
        for p in (out / EXE_NAME, out / 'romfs-index.bin', *ctx.layout.dlls):
            B.copy_file(p, ctx.dest / p.name)
        return {'sha256': B.sha256(ctx.dest / EXE_NAME), 'index_sha256': B.sha256(ctx.dest / 'romfs-index.bin')}


class Shaders(Step):
    """Every NVN 2 stage in the game's files, translated into the shader cache before the first launch: the census
    (tools/shader_census.py) writes seed-nvn2.bin into the install folder, spr-recomp.exe --translate-shaders lowers
    and translates it, --warm-shaders builds the driver's pipeline libraries. The cache belongs to the player."""
    id = 'dkb_shaders'

    def key(self, ctx):
        return B.digest(ctx.keys['compile'], ctx.keys['dkb_translator'], ctx.keys['dkb_data'],
                        B.sha256(ctx.layout.tools / 'shader_census.py'))

    def current(self, ctx, record):
        return record.get('prepared') is True and (ctx.dest / 'shader-cache').exists()

    def run(self, ctx):
        work = ctx.temp / 'shaders'
        owned_dir(ctx, work)
        run_tool(ctx, self.id, [ctx.layout.python, ctx.layout.tools / 'shader_census.py', '--version', GAME,
                                '--romfs', ctx.info['romfs_dir'], '--exefs', ctx.dest / 'exefs', '--out', work,
                                '--seed', ctx.dest / 'seed-nvn2.bin'], name=f'{self.id}-census')
        run_tool(ctx, self.id, [ctx.dest / EXE_NAME, '--translate-shaders'], cwd=ctx.dest)
        # The pipeline libraries are an optimisation; a driver that crashes building them must not fail the step.
        libraries = True
        try:
            run_tool(ctx, self.id, [ctx.dest / EXE_NAME, '--warm-shaders'], cwd=ctx.dest, name=f'{self.id}-warm')
        except InstallError as error:
            if error.id != 'dkb.tool_failed':
                raise
            libraries = False
            ctx.reporter.warning('warn.dkb_libraries_failed', code=error.params.get('code'),
                                 log=str(error.params.get('log')))
        return {'prepared': True, 'libraries': libraries}


class Prepare(Step):
    id = 'dkb_verify'

    def key(self, ctx):
        return B.digest(ctx.keys['compile'], ctx.keys['dkb_translator'], ctx.state['lang'],
                        B.sha256(ctx.layout.tools / XBOX_TOOL))

    def current(self, ctx, record):
        return record.get('selftest') is True

    def run(self, ctx):
        for name in ('dkb.ini', 'dkb.ini.tmp', 'totk.ini'):
            B.ensure_plain(ctx.dest / name)
        # The runtime reads dkb.ini for this target (runtime/cfg.c), the settings file of every non-TotK build.
        ini.write(ctx.dest, ctx.state['lang'], ctx.layout.ini_example, 'dkb.ini', fallback='totk.ini')
        self.xbox_prompts(ctx)
        run_tool(ctx, self.id, [ctx.dest / EXE_NAME, '--selftest'])
        return {'selftest': True}

    def xbox_prompts(self, ctx):
        """romfs-patches-xbox: the pictogram font with the A/B and X/Y prompts swapped, for pad_layout = xbox_prompts
        (tools/raiders_xbox_prompts.py; runtime/romfs.c overlay_init loads it next to romfs-index.bin). Written on
        every install, so the menu choice works without another run of the setup. Optional: when it fails, the game
        keeps the Switch prompts and the menu says the files are missing, so the setup warns and goes on."""
        out = ctx.dest / XBOX_DIR
        B.ensure_plain(out)
        if out.exists():
            B.plain_tree(out)
            shutil.rmtree(out)
        try:
            run_tool(ctx, self.id, [ctx.layout.python, ctx.layout.tools / XBOX_TOOL, '--romfs', ctx.info['romfs_dir'],
                                    '--out', out], name=f'{self.id}-xbox')
        except InstallError as error:
            if error.id != 'dkb.tool_failed':
                raise
            if out.exists():
                shutil.rmtree(out)
            ctx.reporter.warning('warn.xbox_prompts_failed', code=error.params.get('code'),
                                 log=str(error.params.get('log')))


class Cleanup(Step):
    id, always = 'cleanup', True

    def run(self, ctx):
        # The generated C and the objects stay: a reinstall with a newer runtime then recompiles only the runtime.
        for name in ('data', 'shaders', 'xlate', 'dotnet-home', 'nuget'):
            p = ctx.temp / name
            if p.exists():
                owned_dir(ctx, p)
                p.rmdir()


def all_steps():
    return [Check(), Data(), B.Translator(), Lift(), Compile(), Shaders(), Prepare(), Cleanup()]


def main(options):
    rep = Reporter()
    locks = []
    started = time.monotonic()
    try:
        for name in ('SIGBREAK', 'SIGTERM'):
            if hasattr(signal, name):
                signal.signal(getattr(signal, name), B.cancel)
        B.ensure_plain(options.dest.absolute())
        dest = options.dest.resolve()
        B.ensure_plain(dest / STATE)
        previous = statefile.load(dest, STATE)
        options = copy.copy(options)
        found = layouts.find(options.root)
        # A resume from a checkout reuses the tool paths the last run recorded (B.main says why a release does not).
        # An MSYS2 build is not recorded as tools to reuse: msys2_layout finds them again (its DLL folder is MSYS2's whole
        # bin folder, which must not be copied).
        recorded_tools = (previous or {}).get('build_tools', {})
        if (previous and previous.get('game') == GAME and previous.get('destination') == str(dest) and found.dev and
                options.package is None and Path(recorded_tools.get('toolchain') or '.') != layouts.UCRT.parent):
            for name, value in previous.get('build_tools', {}).items():
                if (name in ('toolchain', 'deps', 'ninja', 'dll_dir', 'dotnet') and getattr(options, name) is None and
                        value and Path(value).exists()):
                    setattr(options, name, Path(value))
        layout = msys2_layout(B.tool_layout(found, options), options)
        raw_base = (options.temp or layout.temp or dest.parent / '.spr-setup').absolute()
        B.ensure_plain(raw_base)
        base = raw_base.resolve()
        tag = hashlib.sha256(str(dest).lower().encode()).hexdigest()[:12]
        # Only characters that need no quoting on compiler and MSBuild command lines (B.main, issue 134).
        safe = ''.join(ch if ch.isascii() and (ch.isalnum() or ch in '._-') else '_' for ch in dest.name)
        temp = base / f'spr-{safe}-{tag}'
        info = preflight(layout, options, dest, temp, rep)
        temp.mkdir(parents=True, exist_ok=True)
        B.write_owner(temp / OWNER, info['owner'])
        locks.append(B.lock_temp(temp))
        dest.mkdir(parents=True, exist_ok=True)
        B.write_owner(dest / DEST_OWNER, {'game': GAME, 'destination': str(dest)})
        locks.append(B.lock_temp(dest))
        B.plain_tree(temp)
        plain_destination(dest, layout.dlls)
        log = temp / 'logs' / time.strftime('install-%Y%m%d-%H%M%S.log')
        B.ensure_plain(log)
        log.parent.mkdir(parents=True, exist_ok=True)
        rep.log_file = log.open('a', encoding='utf-8', buffering=1)
        rep.log_path = log
        ctx = RaidersContext(layout, dest, temp, rep, options, state=info['old'], verify=options.verify)
        ctx.info = info
        ctx.state.update(game=GAME, destination=str(dest), sources=info['sources'])
        ctx.state['build_tools'] = {'toolchain': str(layout.llvm), 'deps': str(layout.deps) if layout.deps else None,
                                    'ninja': str(layout.ninja),
                                    'dll_dir': str(layout.dlls[0].parent) if layout.dlls else None,
                                    'dotnet': str(info['dotnet'])}
        ctx.state['lang'] = ini.canonical_language(options.lang or ctx.state.get('lang') or 'en-US')
        steps = all_steps()
        rep.emit('start', release=layout.release, mode=ctx.mode, dev=layout.dev, dest=str(dest), log=str(log),
                 game=GAME, game_exe=EXE_NAME, steps=[s.id for s in steps])
        result = run_steps(steps, ctx)
    except InstallError as e:
        rep.error(e.id, e.params)
        result = 'failed'
    except statefile.StateError as e:
        rep.error(e.id, {'path': str(e.path)})
        result = 'failed'
    except KeyboardInterrupt:
        result = 'cancelled'
    except (OSError, ValueError, KeyError, TypeError, ImportError) as e:
        rep.error('internal', {'exception': type(e).__name__, 'detail': str(e)})
        result = 'failed'
    rep.emit('end', result=result, seconds=round(time.monotonic() - started, 1))
    rep.close()
    for lock in locks:
        lock.close()
    return {'ok': 0, 'failed': 1, 'cancelled': 3}[result]
