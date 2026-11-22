"""Resolve the Raiders mouse hooks in the ExeFS selected for recompilation."""

import hashlib

from module import Module
from patches import PatchError, Table, bl_target, bl_word, patch_page


UPDATE = 0x245ED40
POSE = 0x245D750
PITCH_MAP = 0x2466510
# Sizes and SHA-256 digests of the 65536 routines after routine_words() normalization.
FINGERPRINTS = {
    UPDATE: (0x6BE0, 'ad92342ace8793c119b9a10932352fe00c66b5b58805f3832b18c630f13d83f6'),
    POSE: (0xFC0, '2fd85c3be65daf021e7e4f67c6548ef92d6231d45a0ad6604d3898a412145278'),
    PITCH_MAP: (0x2C0, '2430c7278d6ae39a74d4a838cb75aa54d4a79ea962adaa3dc0e6bac579a5a0d1'),
}
SLOTS = {
    0x245FFAC: 0x300,  # manual input mode
    0x24603AC: 0x310,  # horizontal orbit
    0x2461D24: 0x320,  # vertical response
    0x2460E70: 0x330,  # pitch map, button camera
    0x2461128: 0x330,  # pitch map, sensor reference
    0x24611B0: 0x330,  # pitch map, neutral reference
}
PITCH_CALLS = (0x2460E74, 0x246112C, 0x24611B4)
POSE_CALLS = (0x2462D24, 0x2462D3C)


def routine_bounds(mod):
    starts = mod.eh_frame_functions()
    return dict(zip(starts, starts[1:] + [mod.hi]))


def routine_words(mod, start, end):
    words = mod.words[(start - mod.lo) // 4:(end - mod.lo) // 4].copy()
    # Preserve local branches, register assignments, constants and field offsets.
    # Calls and address materialization carry offsets that the linker can move.
    calls = words >> 26 == 0x25
    words[calls] &= 0xFC000000
    addresses = (words & 0x1F000000) == 0x10000000
    words[addresses] &= 0x9F00001F
    return words


def locate(target):
    target_bounds = routine_bounds(target)

    def same_routine(old, new):
        size, digest = FINGERPRINTS[old]
        new_end = target_bounds.get(new, new)
        return (target.lo <= new < new_end <= target.hi and new % 4 == 0 and new_end - new == size
                and hashlib.sha256(routine_words(target, new, new_end)).hexdigest() == digest)

    def same_callee(calls, delta, original):
        # same_routine() has already checked that every call is a BL.
        addresses = {bl_target(target.word(pc + delta), pc + delta) for pc in calls}
        if len(addresses) == 1:
            entry = addresses.pop()
            if same_routine(original, entry):
                return entry
        return None

    matches = []
    for start in target_bounds:
        if same_routine(UPDATE, start):
            delta = start - UPDATE
            pitch_entry = same_callee(PITCH_CALLS, delta, PITCH_MAP)
            if pitch_entry is not None and same_callee(POSE_CALLS, delta, POSE) is not None:
                matches.append((delta, pitch_entry))
    if len(matches) != 1:
        raise PatchError(f'expected one compatible player camera, found {len(matches)}')
    return matches[0]


def resolve(target_main, mods):
    target = Module('main', target_main, 0)
    delta, pitch = locate(target)
    base = next(b for n, b, _ in mods if n == 'main')
    page = patch_page(mods)
    sites = {}
    for old, slot in SLOTS.items():
        new = old + delta
        word = target.word(new)
        sites[new] = (0, [word, bl_word(base + new, page + slot)], word)
    return Table([('raidersmouse', 'TOTK_RAIDERS_MOUSE', 1)], {'main': sites}, page), base + pitch
