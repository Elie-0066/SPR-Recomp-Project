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


if __name__ == '__main__':
    unittest.main()
