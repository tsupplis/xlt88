#!/usr/bin/env python3
"""M80 block repetition and EXITM: compare emitted bytes to explicit source."""
from pathlib import Path
import subprocess
import tempfile
from toolrun import exe, source_case

with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    source, output = work/'main.mac', work/'out.asm'

    run, binary = source_case(source, output)

    text = '''n EQU 2
one MACRO value
 LOCAL again
again: mvi a,value
 jmp again
 EXITM
 mystery
 ENDM
outer MACRO value
 REPT 2
  one value
 ENDM
 mvi b,9
 ENDM
 REPT n
  IRP reg,<b,c>
   mvi reg,1
  ENDM
 ENDM
 outer 7
 IRP number,<1,2,3>
  db number
  IFE number-2
   EXITM
  ENDIF
  db 9
 ENDM
 IRPC number,345
  db number
 ENDM
 IRPC number,<67>
  db number
 ENDM
 IRP unused,<>
  db 8 unused
 ENDM
 IRPC unused,<>
  db 9 unused
 ENDM
 REPT 0
  mystery
 ENDM
 REPT 2
  REPT 3
   db 10
   EXITM
   mystery
  ENDM
  db 11
 ENDM
 jmp 0
 END
'''
    flat = work/'flat.asm'
    flat.write_text(''' mvi b,1
 mvi c,1
 mvi b,1
 mvi c,1
first: mvi a,7
 jmp first
second: mvi a,7
 jmp second
 mvi b,9
 db 1,9,2,3,4,5,6,7,8,9,10,11,10,11
 jmp 0
 end
''')
    for target in ('msdos', 'cpm86'):
        run(text, '--target='+target)
        actual = binary()
        subprocess.run([str(exe), '--target='+target, str(flat), str(output)], check=True)
        assert binary() == actual
    run(text, '--emit=8080')
    flat.write_text(output.read_text())
    subprocess.run([str(exe), str(flat), str(output)], check=True)

    # EXITM propagates through included source to its expansion, but not beyond it.
    (work/'leave.inc').write_text(' db 1\n EXITM\n mystery\n')
    run('REPT 3\n INCLUDE leave.inc\n mystery\nENDM\n db 2\n end\n', '--emit=8080')
    assert output.read_text().count('db') == 2

    # Inactive blocks consume matching ENDM, without executing conditions inside.
    run('IF 0\n REPT unknown\n ELSE\n IRP x,<1>\n ENDM\n ENDM\nENDIF\n db 4\n end\n', '--emit=8080')
    assert output.read_text().count('db') == 1
    run('REPT 1\n IF 0\n EXITM\n ENDIF\n db 4\nENDM\n end\n', '--emit=8080')
    assert output.read_text().count('db') == 1

    # Rejected macro operators in discarded code must not defeat EXITM/REPT 0.
    run('skip MACRO\n REPT 0\n db &bad\n ENDM\n EXITM\n db &bad\n ENDM\n skip\n db 3\n end\n', '--emit=8080')
    assert output.read_text().count('db') == 1

    # Comma-containing IRP elements are one substitution, including nested lists.
    run("IRP item,<<1,2>,<'a,b',0>>\n db item\nENDM\n end\n", '--emit=8080')
    assert '1,2' in output.read_text() and "'a,b',0" in output.read_text()
    run('IRP group,<<1,2>,<3,4>>\n IRP item,<group>\n db item\n ENDM\nENDM\n end\n', '--emit=8080')
    assert output.read_text().count('db') == 4

    for text, expected in [
        ('EXITM\n', 'requires an expansion'),
        ('REPT 1\n EXITM 1\nENDM\n', 'no operands'),
        ('REPT -1\nENDM\n', '0 to 65535'),
        ('REPT 65536\nENDM\n', '0 to 65535'),
        ('REPT unknown\nENDM\n', 'known absolute'),
        ('REPT 1\n', 'missing ENDM'),
        ('IRP x,1,2\nENDM\n', 'angle-bracketed'),
        ('IRPC x,<12\nENDM\n', 'unclosed'),
        ('IRP 1,<2>\nENDM\n', 'parameter name'),
        ('IRP x,<1>\n IRP x,<2>\n ENDM\nENDM\n', 'shadowing'),
        ('REPT 1\n LOCAL x\nENDM\n', 'LOCAL'),
        ('REPT 65535\n REPT 2\n ENDM\nENDM\n', 'budget'),
        ('REPT 1\n'*33 + 'ENDM\n'*33, 'nesting exceeds'),
    ]:
        error = run(text, ok=False)
        assert expected in error, (expected, error)
print('M80 REPT/IRP/IRPC, nested EXITM, empty lists, provenance and expansion bounds passed')
