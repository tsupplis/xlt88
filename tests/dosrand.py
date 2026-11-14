"""Opaque FCB random positions: translated source, all four DOS emitters."""
import os
import subprocess
from pathlib import Path
import tempfile
from toolrun import runner, exe


def call(function, status=None):
    text = f' lxi d,file\n mvi c,{function}\n call 5\n'
    if status is not None:
        text += f' cmp l\n jnz failed\n cpi {status}\n jnz failed\n'
    return text


def program(position, status, partial=False):
    # Only native calls touch metadata. Function 36 captures the sequential
    # position; intervening sequential reads must not change that bookmark.
    text = ''' org 100h
 lxi h,5ch
 lxi d,file
 call name
 lxi d,buffer
 mvi c,26
 call 5
'''
    text += call(15) + ' cpi 255\n jz failed\n'
    text += call(20, 0) * position
    text += call(36)
    if status == 0:
        text += call(20, 0)
    text += call(33, status)
    if status == 0:
        text += f' lda buffer\n cpi {position % 251}\n jnz failed\n'
    text += call(33, status)
    if status == 0:
        text += f' lda buffer\n cpi {position % 251}\n jnz failed\n'
    if partial:
        text += ' lxi h,buffer\n' + ' inx h\n' * 7
        text += ' mov a,m\n ora a\n jnz failed\n'
    if status == 0 and os.environ.get('DOS_RSEQ') == '1':
        # Random read leaves sequential position at the same record.
        text += call(20, 0)
        text += f' lda buffer\n cpi {position % 251}\n jnz failed\n'
    text += call(16) + ''' cpi 255
 jz failed
 lxi d,good
 jmp print
failed: lxi d,bad
print: mvi c,9
 call 5
 jmp 0
name: mvi b,12
next: mov a,m
 stax d
 inx h
 inx d
 dcr b
 jnz next
 ret
good: db 'RANDOM OK$'
bad: db 'RANDOM FAILED$'
file: ds 36
buffer: ds 128
 end
'''
    # Distinct failure messages keep assembler/runtime failures actionable.
    lines = text.splitlines()
    failures = []
    for index, line in enumerate(lines):
        if line.strip() in ('jnz failed', 'jz failed'):
            label = f'bad{len(failures)}'
            lines[index] = line.replace('failed', label)
            failures.append(f"{label}: lxi d,msg{len(failures)}\n jmp print\nmsg{len(failures)}: db 'FAILED line {index + 1}$'\n")
    return '\n'.join(lines).replace('good: db', ''.join(failures) + 'good: db') + '\n'


with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    _, build, execute = runner(work, 'msdos')
    # Full records, an extent crossing, padded partial record, and EOF.
    cases = ((0, 2 * 128, 0), (129, 131 * 128, 0),
             (1, 128 + 7, 0), (2, 2 * 128, 1), (0, 0, 1))
    for flavor in ('nasm', 'asm86', 'masm3', 'drasm86'):
        for position, size, status in cases:
            data = bytes((i // 128) % 251 for i in range(size))
            path = work / 'random.dat'
            path.write_bytes(data)
            build('dosrand', program(position, status, size % 128 != 0), flavor)
            output = execute('dosrand', 'random.dat')
            assert b'RANDOM OK' in output and b'FAILED' not in output, (flavor, position, size, output)
            assert path.read_bytes() == data
        continuation = ('passed' if os.environ.get('DOS_RSEQ') == '1' else
                        'execution unverified (DOS_RSEQ=1 requires correct random-read positioning)')
        print(flavor, 'random bookmark, repeat read, extent, padding and EOF passed;',
              'sequential continuation', continuation, flush=True)

    # New services must not bypass metadata checks or enable the second default FCB.
    for function in (33, 36):
        unsafe = f' org 100h\n lxi d,6ch\n mvi c,{function}\n call 5\n jmp 0\n end\n'
        (work / 'unsafe.80').write_text(unsafe)
        (work / 'keep.asm').write_text('preserve')
        result = subprocess.run([str(exe), '--target=msdos', str(work / 'unsafe.80'),
                                 str(work / 'keep.asm')], capture_output=True)
        assert result.returncode and b'default CP/M FCB recognized' in result.stderr, result.stderr
        assert (work / 'keep.asm').read_text() == 'preserve'
    unsafe = program(0, 0).replace(
        ' lxi d,file\n mvi c,36', ' lxi h,file+33\n mvi m,1\n lxi d,file\n mvi c,36')
    (work / 'unsafe.80').write_text(unsafe)
    result = subprocess.run([str(exe), '--target=msdos', str(work / 'unsafe.80'),
                             str(work / 'keep.asm')], capture_output=True)
    assert result.returncode and b'FCB' in result.stderr, result.stderr
    assert (work / 'keep.asm').read_text() == 'preserve'
