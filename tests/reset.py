"""Selected-drive reset: native state test, gated on emulator support."""
import os
from pathlib import Path
import tempfile
from toolrun import runner, env

source = ' org 100h\n'
for drive in (0, 15, 1):
    source += f' mvi e,{drive}\n mvi c,14\n call 5\n mvi c,28\n call 5\n'
source += ''' lxi d,8001h
 mvi c,37
 call 5
 cmp l
 jnz failed
 ora a
 jnz failed
 mvi c,29
 call 5
 mov a,l
 ani 3
 cpi 2
 jnz failed
 mov a,h
 ani 80h
 jnz failed
 mvi c,25
 call 5
 cpi 1
 jnz failed
 lxi d,good
 jmp print
failed: lxi d,bad
print: mvi c,9
 call 5
 jmp 0
good: db 'RESET OK$'
bad: db 'RESET FAILED$'
 end
'''
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    for drive in ('A', 'B', 'P'):
        env['EMU2_DRIVE_' + drive] = str(work)
    _, build, execute = runner(work, 'cpm86')
    for flavor in ('nasm', 'asm86', 'masm3', 'drasm86'):
        build('reset', source, flavor)
        if os.environ.get('CPM_RESET') == '1':
            output = execute('reset')
            assert b'RESET OK' in output and b'FAILED' not in output, output
        print(flavor, 'selected-drive reset assembled; ' +
              ('state execution passed' if os.environ.get('CPM_RESET') == '1' else
               'state execution unverified (CPM_RESET=1 requires native drive state)'), flush=True)
