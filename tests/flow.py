"""Source-boundary, ORG, C propagation and native console acceptance tests."""
import os
from pathlib import Path
import subprocess
import tempfile
from toolrun import runner
root = Path(__file__).resolve().parents[1]
exe = Path(os.environ.get('XLT88',root/'xlt88'))
source = ''' org 100h
 jmp base+1
base: inr b
 jmp $+3
 mvi a,7
 cpi 7
 jnz failed
 lxi h,message
 mov a,h
 cpi 4
 jnz failed
 mov a,l
 ora a
 jnz failed
 call routine+1
 cpi 8
 jnz failed
 lxi b,9
 lxi d,message
 push h
 pop h
 call 5
 jmp 0
failed: jmp 0
routine: inr c
 mvi a,8
 ret
 org 400h
message: db 'FLOW OK$'
 end
'''
status = ''' mvi c,11
 nop
 call 5
 cpi 0
 jnz failed
 mov a,l
 cpi 0
 jnz failed
 mvi c,1
 call 5
 cpi 90
 jnz failed
 mvi c,9
 lxi d,message
 call 5
 jmp 0
failed: jmp 0
message: db 'STATUS OK$'
 end
'''
buffer = ''' mvi c,10
 lxi d,buffer
 call 5
 lda buffer+1
 cpi 3
 jnz failed
 lda buffer+2
 cpi 65
 jnz failed
 lda buffer+4
 cpi 67
 jnz failed
 lda guard
 cpi 85
 jnz failed
 mvi c,9
 lxi d,message
 call 5
 jmp 0
failed: jmp 0
buffer: db 3,0,0,0,0
guard: db 85
message: db 'BUFFER OK$'
 end
'''
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    for target in ('msdos','cpm86'):
        _, build, execute = runner(work,target)
        for flavor in ('nasm','asm86','masm3','drasm86'):
            build('flow',source,flavor)
            assert b'FLOW OK' in execute('flow'), (target,flavor)
            aliases = 'base0 equ 0\nbdos equ 5+base0\nwboot equ base0+0\n' + source.replace('call 5','call bdos').replace('jmp 0','jmp wboot')
            build('alias',aliases,flavor)
            assert b'FLOW OK' in execute('alias'), (target,flavor)
            build('status',status,flavor)
            # emu2 reports redirected stdin as not-ready; verify A=L=0.
            assert b'STATUS OK' in execute('status',input=b'Z\r'), (target,flavor)
            # A fixed ORG must fail assembly when translated code outgrows it.
            try:
                build('overlap',' org 100h\n inr b\n org 101h\n jmp 0\n end\n',flavor)
            except AssertionError:
                pass
            else:
                raise AssertionError((target,flavor,'accepted target overlap'))
            if target == 'cpm86':
                build('buffer',buffer,flavor)
                result = execute('buffer',input=b'ABC\r')
                assert b'BUFFER OK' in result, (target,flavor,result)
                empty = buffer.replace('cpi 3','cpi 0')
                begin, end = empty.index(' lda buffer+2'), empty.index(' lda guard')
                empty = empty[:begin] + empty[end:]
                build('empty',empty,flavor)
                assert b'BUFFER OK' in execute('empty',input=b'\r')
            print(target,flavor,'flow/layout/console passed',flush=True)
    for text, message in [
        (' jmp base+1\nbase: lxi h,1234h\n end\n','transfer target'),
        (' jmp data+1\ndata: db 0,0,0\n end\n','transfer target'),
        (' mvi c,9\nsite: call 5\n end\n','statically proven'),
        (' mvi c,9\n inx b\n call 5\n end\n','statically proven'),
        (' mvi c,9\n pop b\n call 5\n end\n','statically proven'),
        (' mvi c,9\n mov c,a\n call 5\n end\n','statically proven'),
        (' mvi c,9\n call helper\n call 5\nhelper: ret\n end\n','statically proven'),
        (' org 100h\n nop\n org 100h\n end\n','overlap'),
    ]:
        (work/'bad.asm').write_text(text)
        (work/'out.asm').write_text('preserve')
        result=subprocess.run([str(exe),'--target=cpm86',str(work/'bad.asm'),str(work/'out.asm')],capture_output=True,text=True)
        assert result.returncode and message in result.stderr, result.stderr
        assert (work/'out.asm').read_text() == 'preserve'
print('Boundary/control-flow rejections and atomic output preservation passed')
