#!/usr/bin/env python3
"""M80 macro text operators and angle-delimited string conditions."""
from pathlib import Path
import subprocess
import tempfile
from toolrun import exe, source_case

with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    source, output = work/'main.mac', work/'out.asm'
    run, binary = source_case(source, output)

    text = '''value EQU 4
emit MACRO arg
 db '&arg'
 ENDM
branch MACRO suffix
where&suffix: nop
J&suffix where&suffix
 ENDM
choose MACRO arg
 IFB <arg>
  db 1
 ELSE
  IFIDN <arg>,<hello>
   db 2
  ELSE
   db 3
  ENDIF
 ENDIF
 IFNB <arg>
  db 4
 ENDIF
 ENDM
 branch NZ
 emit %value+2
 emit %1<<3
 emit !,
 emit !;
 emit !!
 emit !<
 emit !>
 emit !%
 emit <%1>
 emit <a;b>
 emit <a,b>
 choose
 choose hello
 choose other
 IFIDN <a,b>,<a,b>
  db 5
 ENDIF
 IFDIF <hello>,<Hello>
  db 6
 ENDIF
 IFNB < >
  db 7
 ENDIF
 IFIDN <'a'>,<'a'>
  db 8
 ENDIF
 IFIDN <!>>,<!>>
  db 9
 ENDIF
 IRP number,<%value+1,%value+2>
  emit number
 ENDM
 IRPC ch,<ab>
  db '&ch'
 ENDM
 jmp 0
 END
'''
    flat = work/'flat.asm'
    flat.write_text('''whereNZ: nop
 jnz whereNZ
 db '6','8',',',';','!','<','>','%','%1','a;b','a,b'
 db 1,2,4,3,4,5,6,7,8,9
 db '5','6','a','b'
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

    run("emit MACRO x\n db '&x'\n ENDM\n emit ! \n end\n", '--emit=8080')
    assert "' '" in output.read_text()
    assert 'reserved' in run('jmp __m&80_1\n', ok=False)

    run("IRPC ch,<!;!,!!>\n db '&ch'\nENDM\n end\n", '--emit=8080')
    assert output.read_text().count('db') == 3
    assert "';'" in output.read_text() and "','" in output.read_text() and "'!'" in output.read_text()

    # A retained ampersand permits substitution at each nesting level.
    run('''outer MACRO prefix
 IRP suffix,<1,2>
prefix&suffix: nop
 jmp prefix&suffix
 ENDM
 ENDM
 outer mark
 end
''', '--emit=8080')
    assert 'mark1' in output.read_text() and 'mark2' in output.read_text()

    # Percent is evaluated when a nested call executes, using earlier EQU values.
    run('''emit MACRO x
 db '&x'
 ENDM
outer MACRO
later EQU 12
 emit %later
 ENDM
 outer
 end
''', '--emit=8080')
    assert "'12'" in output.read_text()
    run('''emit MACRO x
 db '&x'
 ENDM
 emit %-1
 emit %65536
 end
''', '--emit=8080')
    assert "'65535'" in output.read_text() and "'0'" in output.read_text()

    # False branches neither evaluate percent expressions nor validate skipped strings.
    run('IF 0\n IFIDN broken\n ENDIF\n emit %unknown\nENDIF\n nop\n end\n', '--emit=8080')
    (work/'ops.inc').write_text('IFIDN <a>,broken\nENDIF\n')
    assert 'ops.inc:1:' in run('INCLUDE ops.inc\n', ok=False)

    for text, expected in [
        ('IFB bare\nENDIF\n', 'angle brackets'),
        ('IFIDN <a>\nENDIF\n', 'two angle groups'),
        ('IFDIF <a>,<b>,<c>\nENDIF\n', 'unexpected text'),
        ('IFB <a\nENDIF\n', 'unclosed'),
        ('emit MACRO x\n db x\n ENDM\n emit %unknown\n', 'previously defined'),
        ('emit MACRO x\n db x\n ENDM\n emit %1/0\n', 'previously defined'),
        ('emit MACRO x\n ENDM\n emit !\n', 'dangling'),
        ('emit MACRO x\n ENDM\n emit <a><b>\n', 'after macro argument'),
        ('IF2\nENDIF\n', 'not implemented'),
    ]:
        assert expected in run(text, ok=False)
print('M80 &, !, %, string conditions, nesting, byte equivalence and diagnostics passed')
