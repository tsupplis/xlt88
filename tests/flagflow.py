"""Execute dead-flag optimisations and paths where auxiliary carry stays live."""
from pathlib import Path
import tempfile
from toolrun import runner
source = ''' org 100h
 lxi h,1234h
 mvi a,10h
 sui 1
 call pure
 cpi 15
 jnz failed
 mvi a,10h
 sui 1
 call pure
 push psw
 pop b
 mov a,c
 cpi 6
 jnz failed
 mov a,b
 cpi 15
 jnz failed
 mov a,l
 cpi 36h
 jnz failed
 mvi a,10h
 call inner
 cpi 15
 jnz failed
 call outer
 push psw
 pop b
 mov a,c
 cpi 6
 jnz failed
 mov a,b
 cpi 15
 jnz failed
 lxi h,8000h
 mvi a,10h
 sui 1
 dcx h
 push psw
 pop b
 mov a,c
 cpi 6
 jnz failed
 mov a,b
 cpi 15
 jnz failed
 mov a,h
 cpi 7fh
 jnz failed
 mov a,l
 cpi 0ffh
 jnz failed
 mvi a,10h
 sui 1
 jz failed
 cpi 15
 jnz failed
 mvi a,10h
 sui 1
 jnz observe
 ora a
observe: push psw
 pop b
 mov a,c
 ani 10h
 jnz failed
 lxi h,1234h
 mvi a,0
 cpi 0
 stc
 inx h
 dcx h
 push psw
 pop b
 mov a,c
 cpi 87
 jnz failed
 mov a,l
 cpi 34h
 jnz failed
 mvi a,0ffh
 ani 15
 cpi 15
 jnz failed
 lxi d,good
 mvi c,9
 call 5
 jmp 0
failed: lxi d,bad
 mvi c,9
 call 5
 jmp 0
pure: inx h
 jmp pureend
pureend: rz
 ret
outer: mvi a,10h
inner: sui 1
 rnz
 ret
good: db 'FLAGS OK$'
bad: db 'FLAGS FAILED$'
 end
'''
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    for target in ('msdos', 'cpm86'):
        _, build, execute = runner(work, target)
        for flavor in ('nasm', 'asm86', 'masm3', 'drasm86'):
            build('flags', source, flavor)
            output = execute('flags')
            assert b'FLAGS OK' in output and b'FAILED' not in output, output
            print(target, flavor, 'dead flags and branch-to-PSW preservation passed', flush=True)
