"""Separated FCB-address/service setup through every DOS emitter."""
import subprocess
from pathlib import Path
import tempfile
from toolrun import runner, root, exe

source = (root / 'tests/copy.asm').read_text().replace(
    'mvi c, 15', 'mvi a, 15\n        nop\n        mov c, a')
source = source.replace('service equ 5', 'service equ 5+0')
source = source.replace('lxi d, input\n        mvi a, 15', 'lxi h, input\n        xchg\n        mvi a, 15')
data = bytes(range(256)) + b'last'
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    _, build, execute = runner(work, 'msdos')
    for flavor in ('nasm', 'asm86', 'masm3', 'drasm86'):
        (work / 'input.dat').write_bytes(data)
        (work / 'output.dat').unlink(missing_ok=True)
        build('fcbflow', source, flavor)
        assert b'COPY OK' in execute('fcbflow', 'input.dat', 'output.dat')
        assert (work / 'output.dat').read_bytes() == data + bytes(-len(data) % 128)
        print(flavor, 'nonadjacent FCB setup and padded copy passed', flush=True)

    # The overlapping second default FCB still needs separate storage analysis.
    for expression in ('6ch',):
        text = f" org 100h\n lxi h,{expression}\n xchg\n mvi c,15\n call 5\n jmp 0\n end\n"
        (work/'unsafe.80').write_text(text)
        (work/'keep.asm').write_text('preserve')
        result = subprocess.run([str(exe), '--target=msdos', str(work/'unsafe.80'), str(work/'keep.asm')], capture_output=True)
        assert result.returncode and b'default CP/M FCB recognized' in result.stderr, result.stderr
        assert (work/'keep.asm').read_text() == 'preserve'
