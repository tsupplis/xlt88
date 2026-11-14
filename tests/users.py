"""Native user-area calls; execution requires an emulator with real user state."""
import os
from pathlib import Path
import tempfile
from toolrun import runner
source = ''' org 100h
 mvi e,255
 mvi c,32
 call 5
 sta original
 mvi e,7
 mvi c,32
 call 5
 mvi e,255
 mvi c,32
 call 5
 cpi 7
 jnz failed
 mvi a,1
 sta passed
failed: lda original
 mov e,a
 mvi c,32
 call 5
 lda passed
 ora a
 jz badmsg
 lxi d,good
 jmp print
badmsg: lxi d,bad
print: mvi c,9
 call 5
 jmp 0
original: db 0
passed: db 0
good: db 'USER OK$'
bad: db 'USER FAILED$'
 end
'''
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    _, build, execute = runner(work,'cpm86')
    for flavor in ('nasm','asm86','masm3','drasm86'):
        build('users',source,flavor)
        if os.environ.get('CPM_USER') == '1':
            output = execute('users')
            assert b'USER OK' in output and b'FAILED' not in output, output
        print(flavor,'user-area calls assembled; '+('execution passed' if os.environ.get('CPM_USER') == '1' else 'execution unverified (CPM_USER=1 requires persistent user state)'),flush=True)
