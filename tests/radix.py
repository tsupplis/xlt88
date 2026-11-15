#!/usr/bin/env python3
"""M80 source radix snapshots, macro formatting and textual NUL."""
from pathlib import Path
import subprocess
import tempfile
from toolrun import exe, source_case
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    source, output, state = work/'main.mac', work/'out.asm', work/'names.sym'
    run, binary = source_case(source, output)

    text = '''emit MACRO arg
 db '&arg'
 ENDM
check MACRO arg
 IF NUL arg
 db 1
 ELSE
 db 2
 ENDIF
 ENDM
 .RADIX 16
first EQU 10
counter SET 10
 db 10,10D,10H,10B,10Q,10O,0x10
 dw 0ffff
 emit %10
 emit %10+1
 emit %10D
 counter SET counter+1
 saved EQU counter
 db saved,counter,'10'
 .RADIX 2
second EQU 10
 db first,second,saved,10
 emit %11
 IF 10 EQ 10B
 db 1
 ENDIF
 .RADIX 10
 db first,second,saved
 check
 check name
 IF NUL unknown,rest
 mystery
 ENDIF
 IF NOT NUL word
 db 3
 ENDIF
 dw NUL
 dw NUL undefined+9
 IF 0
 .RADIX 99
 ENDIF
 jmp 0
 END
'''
    flat = work/'flat.asm'
    flat.write_text('''first equ 16
second equ 2
saved equ 17
 db 16,10,16,2,8,8,16
 dw 65535
 db '10','11','0A',17,17,'10'
 db 16,2,17,2,'11',1
 db 16,2,17,1,2,3
 dw 65535,0
 jmp 0
 end
''')
    for target in ('msdos','cpm86'):
        run(text,'--target='+target)
        actual = binary()
        subprocess.run([str(exe),'--target='+target,str(flat),str(output)],check=True)
        assert binary() == actual
    run(text,'--emit=8080','--state-out='+str(state))
    flat.write_text(output.read_text())
    assert '.radix' not in output.read_text().lower()
    subprocess.run([str(exe),str(flat),str(output)],check=True)
    run(text,'--state-in='+str(state))
    assert 'stale' in run(text.replace('first EQU 10','first EQU 11'),'--state-in='+str(state),ok=False)

    # Changes in included files and macro expansions persist in source order.
    (work/'base.inc').write_text('.RADIX 8\n')
    run('INCLUDE base.inc\n db 10\nchange MACRO\n .RADIX 16\n ENDM\n change\n db 10\n end\n','--emit=8080')
    assert '8D' in output.read_text() and '16D' in output.read_text()
    # Every supported base, independent of the current base for .RADIX itself.
    for radix in range(2,17):
        run(f'.RADIX {radix}\n db 10\n end\n')
        actual=binary()
        flat.write_text(f' db {radix}\n end\n')
        subprocess.run([str(exe),str(flat),str(output)],check=True)
        assert binary()==actual
    # A number deferred by a macro takes the radix at expansion, not definition.
    run('show MACRO\n db 10\n ENDM\n.RADIX 16\n show\n.RADIX 10\n show\n end\n','--emit=8080')
    assert '16D' in output.read_text() and '\tdb\t10' in output.read_text()

    for text,message in [
        ('.RADIX 1\n','2 to 16'),
        ('.RADIX 17\n','2 to 16'),
        ('.RADIX missing\n','known decimal'),
        ('.RADIX 2\n db 2\n','invalid numeric'),
        ('.RADIX 8\n db 9\n','invalid numeric'),
        ('.RADIX 16\n db 0GG\n','invalid numeric'),
        ('emit MACRO x\n db x\n ENDM\n.RADIX 16\n emit %27D\n','ambiguous'),
    ]:
        assert message in run(text,ok=False)
    (work/'base.inc').write_text('.RADIX 1\n')
    assert 'base.inc:1:' in run('INCLUDE base.inc\n',ok=False)
print('M80 radix 2..16, source-order literals, %, NUL, round trips and diagnostics passed')
