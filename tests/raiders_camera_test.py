"""Run the manual orbit hooks against the local update 65536 pitch map, without starting the game."""
import argparse
from pathlib import Path
import subprocess
import sys

import numpy as np

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / "recomp"))
import gen
from patches import PatchError, bl_word
from raiders_mouse import locate, PITCH_CALLS, PITCH_MAP, POSE, POSE_CALLS, routine_bounds, UPDATE


def check_relocation(reference):
    bounds = routine_bounds(reference)
    relocated = {POSE: 0x1000, UPDATE: 0x3000, PITCH_MAP: 0xB000}
    image = bytearray(0xC000)
    starts = []
    for old, new in relocated.items():
        body = reference.image[old:bounds[old]]
        image[new:new + len(body)] = body
        starts.extend((new, new + len(body)))
    delta = relocated[UPDATE] - UPDATE
    for calls, routine in ((PITCH_CALLS, PITCH_MAP), (POSE_CALLS, POSE)):
        for pc in calls:
            site = pc + delta
            image[site:site + 4] = bl_word(site, relocated[routine]).to_bytes(4, 'little')

    class RelocatedMain:
        lo, hi = 0, len(image)
        words = np.frombuffer(image, dtype='<u4')

        def eh_frame_functions(self):
            return sorted(starts)

        def word(self, address):
            return int(self.words[address // 4])

    target = RelocatedMain()
    assert locate(target) == (delta, relocated[PITCH_MAP])
    # Keep the call sites and change the pitch helper's first instruction.
    image[relocated[PITCH_MAP]] ^= 1
    try:
        locate(target)
    except PatchError:
        print('PASS: camera and helpers located after relocation, modified helper rejected', flush=True)
    else:
        raise AssertionError('modified pitch map was accepted')

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--exefs", type=Path, required=True)
parser.add_argument("--cc", required=True)
parser.add_argument("--deps", type=Path, required=True)
args = parser.parse_args()
out = root / "build" / "raiders-camera-test"
out.mkdir(parents=True, exist_ok=True)
check_relocation(gen.Module("main", args.exefs / "main", 0))
mod = gen.Module("main", args.exefs / "main", 0x7000000000)
entry, end = 0x7002466510, 0x70024667d0
source = out / "pitch-map.c"
source.write_text(gen.chunk_text("main", 0, gen.Emitter(mod, {entry}), [(entry, end)]))
binary = out / "camera-test.exe"
subprocess.run([args.cc, "-O2", "-I", root / "runtime", "-I", args.deps / "include",
                "-I", args.deps / "include" / "vma", root / "tests" / "raiders_mouse.c",
                source, "-o", binary], check=True)
subprocess.run([binary], check=True)
