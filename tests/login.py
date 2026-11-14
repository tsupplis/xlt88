"""Native CP/M-86 login-vector result, with isolated mapped drives."""
from pathlib import Path
import tempfile
from toolrun import runner, env
source = ''' org 100h
 mvi c,24
 call 5
 mov a,h
 ani 128
 cpi 128
 jnz failed
 mov a,l
 ani 7
 cpi 7
 jnz failed
 lxi d,good
 mvi c,9
 call 5
 jmp 0
failed: lxi d,bad
 mvi c,9
 call 5
 jmp 0
good: db 'LOGIN OK$'
bad: db 'LOGIN FAILED$'
 end
'''
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    for drive in ('A', 'B', 'P'):
        (work/drive).mkdir()
        env['EMU2_DRIVE_' + drive] = str(work/drive)
    env['EMU2_DRIVE_C'] = str(work)
    env['EMU2_DEFAULT_DRIVE'] = 'C:'
    _, build, execute = runner(work, 'cpm86')
    for flavor in ('nasm', 'asm86', 'masm3', 'drasm86'):
        build('login', source, flavor)
        output = execute('login')
        assert b'LOGIN OK' in output and b'FAILED' not in output, output
        print(flavor, 'CP/M-86 login bitmap in HL passed', flush=True)
