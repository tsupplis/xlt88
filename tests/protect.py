"""Native drive protection; stateful execution needs a supporting emulator."""
import os
from pathlib import Path
import tempfile
from toolrun import runner, env

query = ''' org 100h
 mvi c,29
 call 5
 mov a,h
 ora l
 jnz failed
 lxi d,good
 jmp print
failed: lxi d,bad
print: mvi c,9
 call 5
 jmp 0
good: db 'VECTOR OK$'
bad: db 'VECTOR FAILED$'
 end
'''
# Select drive A in the isolated test mapping, then check its bitmap bit.
protect = query.replace(' mvi c,29', ''' mvi e,0
 mvi c,14
 call 5
 mvi c,28
 call 5
 mvi c,29''').replace(' mov a,h\n ora l\n jnz failed',
                      ' mov a,l\n ani 1\n jz failed')
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    env['EMU2_DRIVE_A'] = str(work)
    _, build, execute = runner(work, 'cpm86')
    for flavor in ('nasm', 'asm86', 'masm3', 'drasm86'):
        build('roquery', query, flavor)
        output = execute('roquery')
        assert b'VECTOR OK' in output and b'FAILED' not in output, output
        build('protect', protect, flavor)
        if os.environ.get('CPM_PROT') == '1':
            output = execute('protect')
            assert b'VECTOR OK' in output and b'FAILED' not in output, output
        print(flavor, 'read-only zero bitmap passed; protection state ' +
              ('passed' if os.environ.get('CPM_PROT') == '1' else
               'execution unverified (CPM_PROT=1 requires native protection state)'), flush=True)
