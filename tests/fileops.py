#!/usr/bin/env python3
"""Execute automatically translated CP/M-80 file operations on CP/M-86."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
emu = os.environ.get('EMU2', 'emu2')
packer = os.environ.get('BIN2CMD', 'bin2cmd')
env = dict(os.environ, EMU2_CPM_VER='2.2', EMU2_DOSVER='1.0')

with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)

    def build(name, text):
        source = work / (name + '.80')
        source.write_text(text)
        subprocess.run([str(root / 'xlt88'), '--target=cpm86', str(source), str(work / (name + '.asm'))], check=True)
        subprocess.run(['nasm', '-Werror', '-f', 'bin', name + '.asm', '-o', name + '.bin'], cwd=work, check=True)
        subprocess.run([packer, name + '.bin', name + '.cmd'], cwd=work, check=True, capture_output=True)

    def execute(name, *args, expected):
        result = subprocess.run([emu, name + '.cmd', *args], cwd=work, env=env,
                                capture_output=True, timeout=20)
        assert result.returncode == 0 and expected in result.stdout, (name, result.stdout, result.stderr)

    copy = (root / 'tests/copy.asm').read_text()
    # Check the documented byte result in both source registers after every
    # file call. The original source remains an unchanged translation fixture.
    copy = copy.replace('        org 100h', '        org 100h\n        lxi sp, stacktop')
    copy = copy.replace('        call bdos\n        cpi', '        call bdos\n        cmp l\n        jnz error\n        cpi')
    copy = copy.replace('        call bdos\n        ora a', '        call bdos\n        cmp l\n        jnz error\n        ora a')
    copy = copy.replace('        end', '        ds 128\nstacktop:\n        end')
    # Exercise conditional native file calls.
    copy = copy.replace('        mvi c, 20\n        call bdos',
                        '        xra a\n        mvi c, 20\n        cz bdos')

    build('copy', copy)
    # The removed layout assertion must remain unavailable.
    result = subprocess.run([str(root / 'xlt88'), '--target=msdos', '--fcb=dos',
                             str(work / 'copy.80'), str(work / 'bad.asm')], capture_output=True)
    assert result.returncode and b'usage:' in result.stderr

    for records in (0, 1, 127, 128, 129, 260):
        data = bytes((i * 37 + i // 128) & 255 for i in range(records * 128))
        (work / 'input.dat').write_bytes(data)
        (work / 'output.dat').unlink(missing_ok=True)
        execute('copy', 'input.dat', 'output.dat', expected=b'COPY OK')
        assert (work / 'output.dat').read_bytes() == data, records
    execute('copy', 'missing.dat', 'absent.dat', expected=b'COPY FAILED')
    assert not (work / 'absent.dat').exists()

    def program(body, declarations):
        return '''        org 100h
        lxi sp, stacktop
''' + body + '''        lxi d, okay
        jmp print
failed: lxi d, error
print:  mvi c, 9
        call 5
        jmp 0
okay:   db 'FILE OK$'
error:  db 'FILE FAILED$'
''' + declarations + '''        ds 128
stacktop:
        end
'''

    def call(function, label, absent=False):
        return f'''        lxi d, {label}
        mvi c, {function}
        call 5
        cmp l
        jnz failed
        cpi 0ffh
        {'jnz' if absent else 'jz'} failed
'''

    rename_fcb = "rename: db 0,'OUTPUT  DAT'\n        ds 4\n        db 0,'RENAMED DAT'\n        ds 20\n"
    build('rename', program(call(23, 'rename'), rename_fcb))
    before = (work / 'output.dat').read_bytes()
    execute('rename', expected=b'FILE OK')
    assert not (work / 'output.dat').exists()
    assert (work / 'renamed.dat').read_bytes() == before
    # A second rename of the now-absent original must report FFh.
    build('renfail', program(call(23, 'rename', absent=True), rename_fcb))
    execute('renfail', expected=b'FILE OK')
    renamed_fcb = "file:   db 0,'RENAMED DAT'\n        ds 24\n"
    body = call(15, 'file') + call(16, 'file') + call(19, 'file')
    body += call(15, 'file', absent=True) + call(19, 'file', absent=True)
    build('delete', program(body, renamed_fcb))
    execute('delete', expected=b'FILE OK')
    assert not (work / 'renamed.dat').exists()

print('Translated CP/M-86 file operations: 0/1/127/128/129/260 records, EOF, A=L, missing files, rename and delete passed')
