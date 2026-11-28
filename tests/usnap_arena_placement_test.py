"""CPU check of where vk_usnap.c arena_get asks VMA to place a uniform-snapshot arena; no Vulkan loader or device.

Usage: python tests/usnap_arena_placement_test.py --cc /path/to/clang --deps /path/to/runtime/deps

Why it matters: every submission copies the uniform ranges it binds into an arena (vk_usnap.c). By default the arena
prefers device-local memory, which VMA serves from the BAR when the memory must be host-visible. Without resizable BAR
on a Thunderbolt eGPU, that BAR is a 256 MiB window across the cable, and the per-draw copies are then writes over the
link. TOTK_USNAP_HOST=1 must move the arena to host memory, and leaving it unset (or giving any other value, "ab"
included, which rt_knob reports as 2) must keep the default placement. The fixture includes vk_usnap.c itself and
replaces vmaCreateBuffer, so it checks the VmaAllocationCreateInfo that arena_get really builds. It does not show which
memory type a real driver picks for either request, or what either costs.
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

FIXTURE = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vk_usnap.c"

static unsigned checks, failures;
#define CHECK(c) do { checks++; if (!(c)) { failures++; fprintf(stderr, "usnap check line %d\n", __LINE__); } } while (0)

static VkCtx ctx;
static VmaMemoryUsage seen_usage;
static VmaAllocationCreateFlags seen_flags;
static unsigned creates;
static unsigned char backing[ARENA_MIN];

VkCtx *vk_ctx(void) { return &ctx; }
VkResult vmaCreateBuffer(VmaAllocator al, const VkBufferCreateInfo *bi, const VmaAllocationCreateInfo *ai, VkBuffer *buf,
                         VmaAllocation *alloc, VmaAllocationInfo *info) {
    (void)al;
    creates++;
    seen_usage = ai->usage, seen_flags = ai->flags;
    CHECK(bi->usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    CHECK(bi->size <= sizeof backing);
    *buf = (VkBuffer)(uintptr_t)1, *alloc = (VmaAllocation)(uintptr_t)1;
    memset(info, 0, sizeof *info);
    info->pMappedData = backing;
    info->memoryType = ai->usage == VMA_MEMORY_USAGE_AUTO_PREFER_HOST ? 1 : 0;
    return VK_SUCCESS;
}
int rt_knob(const char *name, const char *def) {   // the same reading as perf.c rt_knob, from the environment
    char key[64];
    snprintf(key, sizeof key, "TOTK_%s", name);
    const char *e = getenv(key);
    if (!e || !*e) e = def;
    return !strcmp(e, "ab") ? 2 : strcmp(e, "0") != 0;
}
void rt_log(const char *fmt, ...) { (void)fmt; }
_Noreturn void rt_fatal(const char *fmt, ...) { fprintf(stderr, "rt_fatal: %s\n", fmt); abort(); }
'''

MAIN = r'''
int main(int argc, char **argv) {
    // argv[1]: the usage arena_get must ask for, "host" or "device"
    ctx.mem_props.memoryTypeCount = 2;
    ctx.mem_props.memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    ctx.mem_props.memoryTypes[1].propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    int want_host = argc > 1 && !strcmp(argv[1], "host");
    for (int i = 0; i < 2; i++) {   // the knob is read once; a second arena must get the same placement
        Arena *a = arena_get(1024);
        CHECK(a && a->map == backing);
        CHECK(seen_usage == (want_host ? VMA_MEMORY_USAGE_AUTO_PREFER_HOST : VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE));
        // the replay writes the copy through the mapping in either placement
        CHECK(seen_flags & VMA_ALLOCATION_CREATE_MAPPED_BIT);
        CHECK(seen_flags & VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
        free(a);
    }
    CHECK(creates == 2);
    printf("%u checks, %u failures\n", checks, failures);
    return failures != 0;
}
'''

# The other symbols vk_usnap.c refers to; arena_get does not reach them.
STUBS = r'''
uint64_t kern_now_ns(void) { abort(); }
void plat_mutex_lock(PlatMutex *m) { (void)m; }
void plat_mutex_unlock(PlatMutex *m) { (void)m; }
'''

CASES = [  # (TOTK_USNAP_HOST, expected placement)
    (None, 'device'),
    ('0', 'device'),
    ('1', 'host'),
    ('ab', 'device'),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default=os.environ.get('CC') or shutil.which('clang'))
    parser.add_argument('--deps', type=Path, default=ROOT / 'runtime/deps')
    args = parser.parse_args()
    if not args.cc:
        parser.error('pass --cc with the project Clang executable')
    include = args.deps / 'include' if (args.deps / 'include').is_dir() else args.deps
    vma = next((p.parent for p in include.rglob('vk_mem_alloc.h')), None) if include.is_dir() else None
    if vma is None:
        parser.error('--deps must contain vk_mem_alloc.h (directly or under include/)')
    with tempfile.TemporaryDirectory() as tmp:
        src = Path(tmp) / 'usnap_fixture.c'
        src.write_text(FIXTURE + STUBS + MAIN)
        exe = Path(tmp) / ('usnap_fixture.exe' if os.name == 'nt' else 'usnap_fixture')
        command = [args.cc, '-std=gnu11', '-O1', '-w', f'-I{ROOT / "runtime"}', f'-I{include}', f'-I{vma}',
                   '-Wl,--unresolved-symbols=ignore-all', str(src), '-o', str(exe)]
        built = subprocess.run(command, capture_output=True, text=True, timeout=120)
        assert built.returncode == 0, built.stdout + built.stderr
        for value, want in CASES:
            env = {k: v for k, v in os.environ.items() if k != 'TOTK_USNAP_HOST'}
            if value is not None:
                env['TOTK_USNAP_HOST'] = value
            ran = subprocess.run([str(exe), want], capture_output=True, text=True, timeout=20, env=env)
            count = re.search(r'(\d+) checks, (\d+) failures', ran.stdout)
            assert count and int(count[2]) == 0 and ran.returncode == 0, (value, ran.stdout + ran.stderr)
            print(f'TOTK_USNAP_HOST={value!s:5} -> {want}: {count[1]} checks pass')
    print('usnap arena placement: ok')


if __name__ == '__main__':
    main()
