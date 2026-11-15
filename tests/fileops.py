#!/usr/bin/env python3
"""Execute automatically translated CP/M-80 file operations on CP/M-86."""
from pathlib import Path
import subprocess
import tempfile

from toolrun import root, exe, runner

with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)

    _, native_build, native_execute = runner(work, 'cpm86')

    def build(name, text):
        native_build(name, text, 'nasm')

    def execute(name, *args, expected):
        output = native_execute(name, *args, timeout=20)
        assert expected in output, (name, output)

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
    result = subprocess.run([str(exe), '--target=msdos', '--fcb=dos',
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
