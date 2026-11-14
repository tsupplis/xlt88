"""Execute near/far conditional branches on each emitter and OS."""
from pathlib import Path
import tempfile
from toolrun import runner
source = ''' org 100h
 mvi b,5
again: dcr b
 jnz again
 jz forward
 jmp failed
forward: mvi a,7
 cpi 7
 jnz failed
 jz faraway
failed: jmp 0
 ds 300
faraway: cpi 7
 jnz failed
 mvi c,9
 lxi d,message
 call 5
 jmp 0
message: db 'BRANCH OK$'
 end
'''
with tempfile.TemporaryDirectory() as directory:
    work=Path(directory)
    for target in ('msdos','cpm86'):
        _,build,execute=runner(work,target)
        for flavor in ('nasm','asm86','masm3','drasm86'):
            build('branch',source,flavor)
            assert b'BRANCH OK' in execute('branch'), (target,flavor)
            print(target,flavor,'near/backward/far branches and flags passed',flush=True)
