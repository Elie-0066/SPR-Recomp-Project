"""installer/raiders.py path_overlap: each pair of overlapping folders gets its own message (SPR issue #27).

    python tests/installer_raiders_test.py
"""
import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
from installer import raiders as R  # noqa: E402


class PathOverlap(unittest.TestCase):
    base = Path('/games').resolve()

    def check(self, dest, temp, inputs, expected):
        error = R.path_overlap(self.base / dest, self.base / temp, [self.base / p for p in inputs])
        self.assertEqual(error.id if error else None, expected)

    def test_separate_folders_pass(self):
        self.check('SPR', '.spr-setup/spr-SPR-1', ['raiders.nup'], None)
        self.check('SPR', 'setup/temp/spr-SPR-1', ['dump/exefs', 'dump/romfs'], None)

    def test_install_folder_around_the_setup(self):
        # The Windows release's work folder is <release>/temp: an install folder that is the release folder holds it.
        self.check('setup', 'setup/temp/spr-setup-1', ['raiders.nup'], 'spr.dest_holds_setup')
        self.check('', 'setup/temp/spr-x-1', [], 'spr.dest_holds_setup')

    def test_install_folder_around_the_dump(self):
        self.check('', '.spr-setup/x', ['raiders.nup'], 'spr.dest_holds_setup')
        self.check('SPR', 'tmp/x', ['SPR/raiders.nup'], 'spr.dest_holds_source')
        self.check('dump/SPR', 'tmp/x', ['dump'], 'spr.dest_holds_source')

    def test_dump_in_the_work_folder(self):
        self.check('SPR', 'setup/temp/x', ['setup/temp/x/raiders.nup'], 'spr.setup_holds_source')

    def test_catalog_has_the_messages(self):
        catalog = json.loads((ROOT / 'installer' / 'text' / 'en.json').read_text(encoding='utf-8'))
        for key in ('spr.dest_holds_setup', 'spr.dest_holds_source', 'spr.setup_holds_source'):
            self.assertIn('{path}', catalog[key])
            self.assertIn('{other}', catalog[key])


class OwnRomfs(unittest.TestCase):
    """A RomFS folder lands in the install folder, which then runs without the dump (SPR issue #32)."""

    def test_tree_linked_or_copied(self):
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            src, dst = Path(tmp) / 'dump' / 'romfs', Path(tmp) / 'SPR' / '.setup-nup' / 'romfs'
            (src / 'Shader').mkdir(parents=True)
            (src / 'Shader' / 'a.bin').write_bytes(b'shader')
            (src / 'Pack.zs').write_bytes(b'pack')
            R.link_or_copy_tree(src, dst)
            self.assertEqual(R.romfs_listing(dst), R.romfs_listing(src))
            self.assertTrue(R.romfs_ok(dst))
            self.assertTrue(R.same_device(src, Path(tmp) / 'SPR' / 'not-yet'))
            for p in src.rglob('*'):   # the install keeps its RomFS when the dump goes
                if p.is_file():
                    p.unlink()
            self.assertEqual((dst / 'Shader' / 'a.bin').read_bytes(), b'shader')

    def test_update_uses_own_romfs_when_dump_is_gone(self):
        import tempfile
        from types import SimpleNamespace
        with tempfile.TemporaryDirectory() as tmp:
            dest = Path(tmp) / 'SPR'
            (dest / 'exefs').mkdir(parents=True)
            (dest / 'exefs' / 'main').write_bytes(b'')
            (dest / 'romfs' / 'Shader').mkdir(parents=True)
            dump = Path(tmp) / 'dump'
            old = {'sources': {'exefs': str(dump / 'exefs'), 'romfs': str(dump / 'romfs')}}
            none = SimpleNamespace(exefs=None, romfs=None, nup=None)
            self.assertTrue(R.own_data(dest, old, none))
            (dump / 'exefs').mkdir(parents=True)
            (dump / 'romfs').mkdir(parents=True)
            self.assertFalse(R.own_data(dest, old, none))   # the dump is still there: rebuild from it


class DirectMouseLift(unittest.TestCase):
    def test_setup_requests_detection_and_warns_when_unavailable(self):
        import tempfile
        from types import SimpleNamespace
        from unittest.mock import Mock, patch

        with tempfile.TemporaryDirectory() as tmp:
            dest = Path(tmp) / 'SPR'
            ctx = SimpleNamespace(dest=dest, temp=Path(tmp) / 'work', reporter=Mock(),
                                  layout=SimpleNamespace(python=sys.executable, recomp=ROOT / 'recomp'),
                                  options=SimpleNamespace(jobs=2))
            header = ctx.temp / 'gen' / 'patches.h'
            for available in (True, False):
                with self.subTest(available=available):
                    ctx.reporter.reset_mock()
                    response = '#define RT_RAIDERS_PITCH_MAP 0x7002466510ull\n' if available else ''
                    with patch.object(R, 'plain_outputs'), patch.object(
                            R, 'run_tool', side_effect=lambda *args: header.write_text(response)) as run_tool:
                        R.Lift().run(ctx)
                    self.assertIn('--raiders-mouse', run_tool.call_args.args[2])
                    if available:
                        ctx.reporter.warning.assert_not_called()
                    else:
                        ctx.reporter.warning.assert_called_once_with('warn.spr_direct_mouse_unavailable')


if __name__ == '__main__':
    unittest.main()
