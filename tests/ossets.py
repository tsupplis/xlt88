"""Finite service sets, register copies and conservative rejection."""
from pathlib import Path
import subprocess
import tempfile
from toolrun import runner, exe
source = ''' org 100h
 mvi a,CHOICE
 ora a
 jz string
 mvi a,2
 mvi e,'X'
 jmp joined
string: mvi a,9
 lxi d,msg
joined: mov c,a
 call 5
 jmp 0
msg: db 'STRING$'
 end
'''
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    for target in ('msdos','cpm86'):
        _, build, execute = runner(work,target)
        for flavor in ('nasm','asm86','masm3','drasm86'):
            for choice, expected in ((0,b'STRING'),(1,b'X')):
                build('ossets',source.replace('CHOICE',str(choice)),flavor)
                output = execute('ossets')
                assert expected in output, (target,flavor,output)
            build('ossets'," org 100h\n mvi a,8\n inr a\n mov c,a\n lxi d,msg\n call 5\n jmp 0\nmsg: db 'COPY$'\n end\n",flavor)
            assert b'COPY' in execute('ossets')
            shared = " org 100h\n mvi a,2\n mvi e,'X'\n call helper\n mvi a,9\n lxi d,msg\n call helper\n jmp 0\nhelper: mov c,a\n jmp 5\nmsg: db 'SHARED$'\n end\n"
            build('ossets',shared,flavor)
            assert b'XSHARED' in execute('ossets')
            print(target,flavor,'both service-set paths and register-copy execution passed',flush=True)
    # A loop grows beyond the bounded set; no guessed dispatcher is emitted.
    overflow = " org 100h\n mvi c,1\nloop: inr c\n jnz loop\n call 5\n end\n"
    for text in (source.replace('mvi a,2','lda msg'), source.replace('mvi a,2','mvi a,38'), overflow):
        (work/'bad.80').write_text(text.replace('CHOICE','1'))
        (work/'out.asm').write_text('preserve')
        result = subprocess.run([str(exe),'--target=msdos',str(work/'bad.80'),str(work/'out.asm')],capture_output=True)
        assert result.returncode and (work/'out.asm').read_text() == 'preserve', result.stderr
print('DOS unknown and unsupported alternatives rejected atomically')
