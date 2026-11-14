"""Native auxiliary/list output assembly acceptance (device I/O not emulated)."""
from pathlib import Path
import tempfile
from toolrun import runner
source = " org 100h\n mvi c,3\n call 5\n mvi e,'A'\n mvi c,4\n call 5\n mvi e,'L'\n mvi c,5\n call 5\n jmp 0\n end\n"
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    for target in ('msdos','cpm86'):
        _, build, _ = runner(work,target)
        for flavor in ('nasm','asm86','masm3','drasm86'):
            build('devices',source,flavor)
            print(target,flavor,'auxiliary/list native calls assembled; device execution unverified',flush=True)
