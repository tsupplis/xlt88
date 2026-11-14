"""Native reset composition: drive A, DMA 80h and dollar-file result."""
from pathlib import Path
import tempfile
from toolrun import runner, env

source = ''' org 100h
 mvi e,1
 mvi c,14
 call 5
 mvi a,85
 sta alternate
 lxi d,alternate
 mvi c,26
 call 5
 call reset
 call reset
 cmp l
 jnz failed
 cpi EXPECT
 jnz failed
 mvi c,25
 call 5
 ora a
 jnz failed
 lxi d,5ch
 mvi c,15
 call 5
 cpi 255
 jz failed
 lxi d,5ch
 mvi c,20
 call 5
 ora a
 jnz failed
 lda 80h
 cpi 'Z'
 jnz failed
 lda alternate
 cpi 85
 jnz failed
 lxi d,5ch
 mvi c,16
 call 5
 lxi d,good
 jmp print
failed: lxi d,bad
print: mvi c,9
 call 5
 jmp 0
reset: mvi c,13
 call 5
 ret
alternate: ds 128
good: db 'RESET OK$'
bad: db 'RESET FAILED$'
 end
'''
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    for drive in ('A', 'B', 'C'):
        env['EMU2_DRIVE_' + drive] = str(work)
    (work / 'input.dat').write_bytes(b'Z' * 128)
    for target in ('cpm86', 'msdos'):
        _, build, execute = runner(work, target)
        for flavor in ('nasm', 'asm86', 'masm3', 'drasm86'):
            for name, expected in ((None, 0), ('ordinary.dat', 0), ('$flag.dat', 255)):
                for old in ('ordinary.dat', '$flag.dat'):
                    (work / old).unlink(missing_ok=True)
                if name:
                    (work / name).write_bytes(b'unchanged')
                build('allreset', source.replace('EXPECT', str(expected)), flavor)
                output = execute('allreset', 'input.dat')
                assert b'RESET OK' in output, (target, flavor, name, output)
                assert (work / 'input.dat').read_bytes() == b'Z' * 128
                if name:
                    assert (work / name).read_bytes() == b'unchanged'
            print(target, flavor, 'reset: A drive, DMA 80h, A=L dollar flag and file preservation passed', flush=True)
