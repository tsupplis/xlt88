"""Separated FCB/DMA declarations retain growth, extent and alias safeguards."""
from pathlib import Path
import subprocess
import tempfile
from toolrun import runner, root, exe

original = (root / 'tests/copy.asm').read_text()
split = original.replace('input:  ds 36', 'input:\n; source FCB\n\n ds 33')
split = split.replace('output: ds 36', 'output:\n; destination FCB\n ds 33')
split = split.replace('buffer: ds 128', 'buffer:\n; native DMA storage\n ds 128')
assert split != original
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    _, build, execute = runner(work, 'msdos')
    for flavor in ('nasm', 'asm86', 'masm3', 'drasm86'):
        build('split', split, flavor)
        payload = bytes(range(256)) * 129
        (work / 'input.dat').write_bytes(payload)
        (work / 'output.dat').unlink(missing_ok=True)
        assert b'COPY OK' in execute('split', 'input.dat', 'output.dat')
        assert (work / 'output.dat').read_bytes() == payload
        print(flavor, 'split FCB/DMA declarations and extent-crossing copy passed', flush=True)
    for change in (
        split.replace('input:\n', 'input:\nalias:\n'),
        split.replace('input:\n', 'input:\n db 0\n'),
        split.replace(' ds 33', ' ds 32', 1),
        split.replace('input:\n', 'input:\n org 2000h\n'),
        split.replace('        lxi h, buffer', '        jmp input\n        lxi h, buffer', 1),
    ):
        (work / 'bad.80').write_text(change)
        (work / 'bad.asm').write_text('preserve')
        result = subprocess.run([str(exe), '--target=msdos', str(work / 'bad.80'), str(work / 'bad.asm')], capture_output=True)
        assert result.returncode and b'FCB analysis' in result.stderr, result.stderr
        assert (work / 'bad.asm').read_text() == 'preserve'
