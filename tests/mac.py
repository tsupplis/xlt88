"""MAC lexical, macro, mutable-symbol and native emission acceptance."""
import os
from pathlib import Path
import subprocess
import tempfile
from toolrun import runner
root=Path(__file__).resolve().parents[1]
exe=Path(os.environ.get('XLT88',root/'xlt88'))
source='''10 $-MACRO
20 V$ER EQU 7$7
FALSE EQU 0
JUMP MACRO ?TO
 IF FALSE
 db 0ffh
 ELSE
 jmp ?TO
 ENDIF
 ENDM
START ORG 100H
 JUMP MAIN
bad: jmp 0
MAIN IF NOT FALSE
 mvi a,VER
 cpi 77
 jnz bad
 ENDIF
COUNT SET 1
 COUNT SET COUNT+1
 IF COUNT EQ 2
 mvi c,9
 lxi d,msg
 call 5
 ELSE
 jmp bad
 ENDIF
 jmp 0
msg: db 'MAC $;!don''t',13,10,'$'
LAST END START
'''
# Use a message without embedded '$' for execution; retain the complete string
# separately as byte-level parser/emitter coverage.
source=source.replace("'MAC $;!don''t'","'MAC don''t'")
mutable=''' base org 100h
 X SET AFTER+1
 dw X
 X SET X+1
 dw X
AFTER: db 0
LAST END base
'''
quotes=''' org 100h
 db 'Dollar$;!don''t',0
 db 'a"b''c',0
 db '''+"''''"+''',0
 end
'''
with tempfile.TemporaryDirectory() as directory:
    work=Path(directory)
    def invoke(text,expected=None,emit='asm80'):
        (work/'input.mac').write_bytes(text if isinstance(text,bytes) else text.encode())
        (work/'output.asm').write_text('preserve')
        result=subprocess.run([str(exe),'--dialect=mac','--emit='+emit,str(work/'input.mac'),str(work/'output.asm')],capture_output=True,text=True)
        if expected:
            assert result.returncode and expected in result.stderr, result.stderr
            assert (work/'output.asm').read_text()=='preserve'
        else:
            assert result.returncode==0,result.stderr
        return result
    invoke(b'10 org 100h!db 1!db 2\r\n end\x1aignored')
    normalized=(work/'output.asm').read_text()
    assert normalized.count('\tdb\t')==2 and 'ignored' not in normalized
    invoke("MAKE MACRO ?NAME\n?NAME EQU 7\n ENDM\n MAKE <FOO$BAR>\n db FOOBAR\n end\n")
    invoke('org 100h\n end missing\n','END entry',emit='nasm')
    invoke('A$B equ 1\nAB equ 2\n end\n','duplicate')
    invoke('ABCDEFGHIJKLMNOPQ equ 1\n end\n','16 significant')
    invoke('x set later\n if x\n db 1\n endif\nlater: db 0\n end\n','previously defined')
    invoke('x equ 1\nx set 2\n end\n','cannot redefine')
    invoke('x equ (65535+1)/2\n end\n','invalid numeric')
    invoke(' if 65535+1\n bad syntax\n endif\n end\n')
    invoke(' org 100h\n nop\nentry: jmp 0\n end entry\n','END entry',emit='nasm')
    invoke(' db '+"'"+'x'*260+"'\n",'254 characters')
    invoke(' org 100h\nx set 65535\ny equ x\nx set x+1\n if x\n db 99\n else\n db low y\n endif\n end\n')
    assert '255' in (work/'output.asm').read_text() or '__mac_set_' in (work/'output.asm').read_text()
    for target in ('msdos','cpm86'):
        _,build,execute=runner(work,target)
        for flavor in ('nasm','asm86','masm3','drasm86'):
            build('mac',source,flavor,dialect='mac')
            assert b"MAC don't\r\n" in execute('mac'), (target,flavor)
            for name,text,expected in [('mutable',mutable,b'\x05\x01\x06\x01\0'),
                                       ('quotes',quotes,b'Dollar$;!don\'t\0a"b\'c\0\x27\0')]:
                build(name,text,flavor,dialect='mac')
                image=(work/(name+('.cmd' if target=='cpm86' else '.com'))).read_bytes()
                if target=='cpm86': image=image[384:]
                assert image[:len(expected)]==expected, (target,flavor,name,image[:len(expected)])
            print(target,flavor,'MAC macros, SET versions, dollar separators and quoted bytes passed',flush=True)
    result=subprocess.run([str(exe),'--dialect=mac','--emit=asm80',str(root/'examples/disk7.mac'),str(work/'disk7.asm')],capture_output=True,text=True)
    assert result.returncode==0,result.stderr
print('Unmodified DISK7 normalization and MAC atomic rejection checks passed')
