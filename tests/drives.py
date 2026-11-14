"""Select native drives and verify zero-drive name records follow selection."""
from pathlib import Path
import tempfile
from toolrun import runner, env

source = ''' org 100h
 mvi e,0
 mvi c,14
 call 5
 mvi c,25
 call 5
 ora a
 jnz failed
 mov a,l
 ora a
 jnz failed
 lxi d,record
 mvi c,19
 call 5
 cpi 255
 jz failed
 mvi e,1
 mvi c,14
 call 5
 mvi c,25
 call 5
 cpi 1
 jnz failed
 mov a,l
 cpi 1
 jnz failed
 lxi d,record
 mvi c,19
 call 5
 cpi 255
 jz failed
 mvi e,2
 mvi c,14
 call 5
 mvi c,25
 call 5
 cpi 2
 jnz failed
 lxi d,good
 mvi c,9
 call 5
 jmp 0
failed: lxi d,bad
 mvi c,9
 call 5
 jmp 0
record: db 0,'SAME    DAT'
 ds 24
good: db 'DRIVES OK$'
bad: db 'DRIVES FAILED$'
 end
'''
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    for drive in ('a','b'):
        (work/drive).mkdir()
        env['EMU2_DRIVE_'+drive.upper()] = str(work/drive)
    env['EMU2_DRIVE_C'] = str(work)
    env['EMU2_DEFAULT_DRIVE'] = 'C:'
    for target in ('msdos','cpm86'):
        _, build, execute = runner(work,target)
        for flavor in ('nasm','asm86','masm3','drasm86'):
            for drive in ('a','b'):
                (work/drive/'same.dat').write_bytes(drive.encode())
                (work/drive/'keep.dat').write_bytes(b'keep')
            (work/'same.dat').write_bytes(b'untouched default C')
            build('drives',source,flavor)
            output = execute('drives')
            assert b'DRIVES OK' in output and b'FAILED' not in output, (target,flavor,output)
            for drive in ('a','b'):
                assert not (work/drive/'same.dat').exists()
                assert (work/drive/'keep.dat').read_bytes() == b'keep'
            assert (work/'same.dat').read_bytes() == b'untouched default C'
            print(target,flavor,'A/B/C selection, current drive and default-drive file access passed',flush=True)
