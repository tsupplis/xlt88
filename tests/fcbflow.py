"""Separated FCB-address/service setup through every DOS emitter."""
from pathlib import Path
import tempfile
from toolrun import runner, root

source = (root / 'tests/copy.asm').read_text().replace(
    'mvi c, 15', 'mvi a, 15\n        nop\n        mov c, a')
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
