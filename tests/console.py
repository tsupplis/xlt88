"""Native BDOS 6 modes and per-initializer location semantics."""
from pathlib import Path
import tempfile
from toolrun import runner

source = ''' mvi c,6
 mvi e,255
 call 5
 cpi 81
 jnz failed
 mov a,l
 cpi 81
 jnz failed
 mvi c,6
 mvi e,65
 call 5
 mvi c,6
 mvi e,254
 call 5
 mvi c,9
 lxi d,msg
 call 5
 jmp 0
failed: jmp 0
msg: db 'DIRECT OK$'
 end
'''
location = ''' org 100h
words: dw $, $
bytes: db 'A,B', $ and 255, $ and 255
 end
'''
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    for target in ('msdos','cpm86'):
        _, build, execute = runner(work,target)
        for flavor in ('nasm','asm86','masm3','drasm86'):
            build('direct',source,flavor)
            output = execute('direct',input=b'Q')
            assert b'A\xfeDIRECT OK' in output, (target,flavor,output)
            build('location',location,flavor)
            if target == 'cpm86':
                binary = (work/'location.cmd').read_bytes()[384:]
            else:
                binary = (work/'location.com').read_bytes()
            assert binary[:9] == b'\x00\x01\x02\x01A,B\x07\x08', (target,flavor,binary[:9])
            print(target,flavor,'direct input/output/FE and data locations passed',flush=True)
