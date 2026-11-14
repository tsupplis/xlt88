#!/usr/bin/env python3
"""Translated CP/M-86 directory and random-record operations."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
emu = os.environ.get('EMU2', 'emu2')
packer = os.environ.get('BIN2CMD', 'bin2cmd')
env = dict(os.environ, EMU2_CPM_VER='2.2')

with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)

    def execute(name, body, data, run=True):
        source = '''        org 100h
        lxi sp, stacktop
        lxi d, buffer
        mvi c, 26
        call 5
''' + body + '''        lxi d, okay
        jmp print
failed: lxi d, error
print:  mvi c, 9
        call 5
        jmp 0
okay:   db 'TEST OK$'
error:  db 'TEST FAILED$'
''' + data + '''buffer: ds 128
        ds 256
stacktop:
        end
'''
        checks = []
        stubs = ''
        lines = source.splitlines()
        for index, line in enumerate(lines):
            if line.strip().endswith(' failed'):
                number = len(checks)
                checks.append((index + 1, lines[max(0, index - 2):index + 1]))
                lines[index] = line.replace(' failed', f' bad{number}')
                stubs += f'bad{number}: lxi d, msg{number}\n        jmp print\nmsg{number}: db "FAIL {number}$"\n'
        source = '\n'.join(lines).replace('okay:   db', stubs + 'okay:   db') + '\n'
        (work / (name + '.80')).write_text(source)
        subprocess.run([str(root / 'xlt88'), '--target=cpm86', name + '.80', name + '.asm'], cwd=work, check=True)
        subprocess.run(['nasm', '-Werror', '-f', 'bin', name + '.asm', '-o', name + '.bin'], cwd=work, check=True)
        subprocess.run([packer, name + '.bin', name + '.cmd'], cwd=work, check=True, capture_output=True)
        if not run:
            assembly = (work / (name + '.asm')).read_text()
            assert 'mov cl, 40' in assembly and 'int 0e0h' in assembly
            return
        result = subprocess.run([emu, name + '.cmd'], cwd=work, env=env, capture_output=True, timeout=20)
        assert result.returncode == 0 and b'TEST OK' in result.stdout, (name, result.stdout, result.stderr, checks)

    def call(n, check='zero'):
        text = f'        lxi d, file\n        mvi c, {n}\n        call 5\n'
        if check:
            text += '        cmp l\n        jnz failed\n'
            text += '        ora a\n        jnz failed\n' if check == 'zero' else '        cpi 0ffh\n        jz failed\n'
        return text

    fcb = "file:   db 0,'RANDOM  DAT'\n        ds 24\n"
    body = call(22, 'directory')
    body += '''        mvi a, 3
        sta file+33
        mvi a, 5ah
        sta buffer
'''
    body += call(40) + call(16, 'directory') + call(35, None)
    body += '''        lda file+33
        cpi 4
        jnz failed
        lda file+34
        ora a
        jnz failed
        lda file+35
        ora a
        jnz failed
        xra a
        sta file+33
'''
    body += call(15, 'directory') + call(33)
    body += '''        lxi h, buffer
        mvi b, 128
check:  mov a, m
        ora a
        jnz failed
        inx h
        dcr b
        jnz check
        mvi a, 3
        sta file+33
'''
    body += call(33)
    body += '''        lda buffer
        cpi 5ah
        jnz failed
'''
    body += call(16, 'directory')
    zero_fill = os.environ.get('CPM_ZFILL') == '1'
    execute('zerofill', body, fcb, run=zero_fill)
    if zero_fill:
        assert (work / 'random.dat').read_bytes() == bytes(384) + b'Z' + bytes(127)
    else:
        print('BDOS 40: emission verified; execution not tested (set CPM_ZFILL=1 with a supporting emulator)')
        (work / 'random.dat').write_bytes(bytes(384) + b'Z' + bytes(127))

    body = call(15, 'directory') + call(20) + call(36, None)
    body += '''        lda file+33
        cpi 1
        jnz failed
        lda file+34
        ora a
        jnz failed
        mvi a, 129
        sta file+33
        mvi a, 0a5h
        sta buffer
'''
    body += call(34) + call(16, 'directory') + call(35, None) + call(15, 'directory')
    body += '''        lda file+33
        cpi 130
        jnz failed
        mvi a, 129
        sta file+33
        xra a
        sta buffer
'''
    body += call(33)
    body += '''        lda buffer
        cpi 0a5h
        jnz failed
'''
    body += call(20)
    body += '''        lda buffer
        cpi 0a5h
        jnz failed
        mvi a, 130
        sta file+33
'''
    body += call(33, None)
    body += '''        cmp l
        jnz failed
        cpi 1
        jz eof
        cpi 4
        jnz failed
eof:    nop
'''
    body += call(16, 'directory')
    execute('random', body, fcb)
    content = (work / 'random.dat').read_bytes()
    assert len(content) == 130 * 128 and content[129 * 128] == 0xa5

    (work / 's1.dat').write_bytes(bytes(128))
    (work / 's2.dat').write_bytes(bytes(128))
    search = '''        lxi d, pattern
        mvi c, 17
        call 5
next:   cmp l
        jnz failed
        cpi 0ffh
        jz done
        cpi 4
        jnc failed
        mov l, a
        mvi h, 0
        dad h
        dad h
        dad h
        dad h
        dad h
        lxi d, buffer
        dad d
        inx h
        mov a, m
        cpi 'S'
        jnz failed
        inx h
        mov a, m
        cpi '1'
        jz one
        cpi '2'
        jnz failed
        lda seen
        ori 2
        jmp save
one:    lda seen
        ori 1
save:   sta seen
        lda count
        inr a
        sta count
        cpi 3
        jnc failed
        mvi c, 18
        call 5
        jmp next
done:   lda count
        cpi 2
        jnz failed
        lda seen
        cpi 3
        jnz failed
'''
    execute('search', search, "pattern: db 0,'S?      DAT'\n        ds 24\nseen: db 0\ncount: db 0\n")
print('Translated CP/M-86 search, random read/write, size, position, EOF passed')
