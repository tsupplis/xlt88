"""Fixed source regions reorder without moving addresses or record identities."""
import os
from pathlib import Path
import subprocess
import tempfile
from toolrun import runner
root = Path(__file__).resolve().parents[1]
exe = Path(os.environ.get('XLT88',root/'xlt88'))
source = ''' org 500h
message: db 'REGIONS OK$'
 org 100h
 jmp next+1
next: inr b
 lxi h,message
 mov a,h
 cpi 5
 jnz failed
 mov a,l
 ora a
 jnz failed
 lda marker
 cpi 85
 jnz failed
 lhld pointer
 mov a,h
 cpi 5
 jnz failed
 mov a,l
 ora a
 jnz failed
 mvi c,9
 lxi d,message
 call 5
 jmp 0
failed: jmp 0
 org 400h
marker: db 85
pointer: dw message
 end
 org 50h
'''
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    for target in ('msdos','cpm86'):
        _, build, execute = runner(work,target)
        for flavor in ('nasm','asm86','masm3','drasm86'):
            build('regions',source,flavor)
            assert b'REGIONS OK' in execute('regions'), (target,flavor)
            print(target,flavor,'fixed regions, pointer data and source boundary passed',flush=True)
    for text, message in [
        (' org 400h\n db 1\n org 100h\n ds 800\n end\n','overlapping'),
        (' org 400h\n dw \'ab\'\n org 100h\n jmp 0\n end\n','known source sizes'),
        (' org 400h\n db 1\n end\n','initial ORG'),
        (' org 100h\n nop\n org 100h\n end\n','overlap'),
        (' org 500h\n db 1\n org 400h\n db 2\n end\n','initial ORG'),
    ]:
        (work/'bad.80').write_text(text)
        (work/'bad.asm').write_text('preserve')
        result = subprocess.run([str(exe),str(work/'bad.80'),str(work/'bad.asm')],capture_output=True,text=True)
        assert result.returncode and message in result.stderr, result.stderr
        assert (work/'bad.asm').read_text() == 'preserve'
print('Region overlap, unknown size, missing entry and atomic rejection passed')
