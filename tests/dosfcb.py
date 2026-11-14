#!/usr/bin/env python3
"""Proved CP/M FCB programs translated to DOS, without layout flags."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
exe = Path(os.environ.get('XLT88', root / 'xlt88'))
emu = os.environ.get('EMU2', 'emu2')
source_text = (root / 'tests/copy.asm').read_text()
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    source, output = work / 'copy.80', work / 'copy.asm'

    def build(text, ok=True):
        source.write_text(text)
        output.write_text('preserve')
        result = subprocess.run([str(exe), '--target=msdos', str(source), str(output)], capture_output=True)
        assert (result.returncode == 0) == ok, result.stderr.decode()
        if not ok:
            assert output.read_text() == 'preserve'
            assert b'FCB analysis' in result.stderr
            return
        subprocess.run(['nasm', '-Werror', '-f', 'bin', 'copy.asm', '-o', 'copy.com'], cwd=work, check=True)

    def run(*args, expected):
        result = subprocess.run([emu, 'copy.com', *args], cwd=work,
                                env=dict(os.environ, EMU2_DOSVER='1.0'), capture_output=True, timeout=20)
        assert result.returncode == 0 and expected in result.stdout, (result.stdout, result.stderr)

    # The first run uses exactly the existing CP/M source, without edits.
    for text in (source_text, source_text.replace('ds 36', 'ds 33'),
                 source_text.replace('mvi c, 22\n        call bdos', 'mvi c, 22\n        cnz bdos')):
        build(text)
        for length in (0, 1, 127, 128, 129, 127*128, 128*128, 129*128, 260*128):
            data = bytes((i * 37 + i // 128) & 255 for i in range(length))
            (work / 'input.dat').write_bytes(data)
            (work / 'output.dat').unlink(missing_ok=True)
            run('input.dat', 'output.dat', expected=b'COPY OK')
            expected = data + bytes((-len(data)) % 128)
            assert (work / 'output.dat').read_bytes() == expected
            run('input.dat', 'output.dat', expected=b'COPY FAILED')
            assert (work / 'output.dat').read_bytes() == expected
            run('input.dat', 'input.dat', expected=b'COPY FAILED')
            assert (work / 'input.dat').read_bytes() == data
        run('missing.dat', 'absent.dat', expected=b'COPY FAILED')
        assert not (work / 'absent.dat').exists()
    # Routine identity is semantic, not tied to fixture-specific names.
    build(source_text.replace('name', 'copyname').replace('input', 'srcfcb').replace('output', 'dstfcb'))
    negatives = [
        source_text.replace('name:   mvi b, 12', 'name:   mvi b, 13'),
        source_text.replace('lxi d, input', 'lxi d, input+1', 1),
        source_text.replace('input:  ds 36', 'input:  ds 12'),
        source_text.replace('        lxi h, buffer', '        lda input+32\n        lxi h, buffer', 1),
        source_text.replace('        lxi h, buffer', '        sta input+12\n        lxi h, buffer', 1),
        source_text.replace('        lxi h, buffer', '        shld packed\n        lxi h, buffer', 1),
        source_text.replace('        lxi h, buffer', '        mvi h, 1\n        mvi l, 32\n        mov a,m\n        lxi h, buffer', 1),
        source_text.replace('        lxi h, buffer', '        jmp next\n        lxi h, buffer', 1),
        source_text.replace('        lxi d, buffer', '        lxi d, input'),
        source_text.replace('        lxi h, buffer', '        lda $+20h\n        lxi h, buffer', 1),
        source_text.replace('        lxi h, buffer', '        lxi sp,input\n        lxi h, buffer', 1),
    ]
    for text in negatives: build(text, ok=False)
print('DOS CP/M-FCB translation: unchanged copy, 33-byte growth, partial records, extent boundaries, no-clobber and unsafe-use rejection passed')
