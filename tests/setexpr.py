#!/usr/bin/env python3
"""SET source-order snapshots and Intel/M80 expression precedence."""
from pathlib import Path
import subprocess
import tempfile
from toolrun import exe, root, source_case
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    source, output, state = work/'main.mac', work/'out.asm', work/'names.sym'
    run, binary = source_case(source, output)

    text = '''count SET 1
saved EQU count+10
 db count
count SET 2
 db count,saved
emit MACRO
 db count
count SET count+1
 ENDM
 REPT 3
 emit
 ENDM
 IF count EQ 5
 db HIGH 1234h,LOW 1234h
 ELSE
 mystery
 ENDIF
 IF 0
count SET 99
 ENDIF
 db count
 dw NOT 0,1 EQ 1,2 NE 2,1 LT 2,1 LE 1,2 GT 1,2 GE 3
 db 1234h SHR 8 + 128,1 SHL 3 + 1,10 MOD 3,1 OR 3 XOR 2
 mvi a,count
A SET 7
 mvi a,A
 mov b,a
 db 'count',count
 jmp 0
 end
'''
    flat = work/'flat.asm'
    flat.write_text('''saved equ 11
 db 1,2,saved,2,3,4,18,52,5
 dw 65535,65535,0,65535,65535,65535,0
 db 146,9,1,1
 mvi a,5
 mvi a,7
 mov b,a
 db 'count',5
 jmp 0
 end
''')
    for target in ('msdos', 'cpm86'):
        run(text, '--target='+target)
        actual = binary()
        subprocess.run([str(exe), '--target='+target, str(flat), str(output)], check=True)
        assert actual == binary()
    run(text, '--emit=8080', '--state-out='+str(state))
    normalized = output.read_text()
    assert "'count'" in normalized and '\tset\t' not in normalized
    flat.write_text(normalized)
    subprocess.run([str(exe), str(flat), str(output)], check=True)
    run(text, '--state-in='+str(state))
    assert 'stale' in run(text.replace('count SET 1', 'count SET 10'), '--state-in='+str(state), ok=False)

    # Frozen numeric operands must remain recognizable to the FCB proof pass.
    copy = (root/'tests/copy.asm').read_text()
    run('length SET 12\n' + copy.replace('mvi b, 12', 'mvi b, length'), '--target=msdos')
    binary()

    # Symbolic/forward operands survive normalization and use the new emitter ops.
    run(''' dw item EQ item,item NE item,item LE item,item GE item
 db HIGH item,LOW item
 jmp 0
item: nop
 end
''')
    binary()
    # A real Tiny BASIC dispatch-table idiom keeps SHR above addition.
    run('LIST EQU 1234h\n db LIST SHR 8 + 128,LIST AND 255\n end\n')
    actual = binary()
    flat.write_text(' db 146,52\n end\n')
    subprocess.run([str(exe), str(flat), str(output)], check=True)
    assert binary() == actual

    (work/'step.inc').write_text('count SET count+1\n db count\n')
    run('count SET 0\n REPT 2\n INCLUDE step.inc\n ENDM\n db count\n end\n', '--emit=8080')
    assert '\tdb\t1' in output.read_text() and output.read_text().count('\tdb\t2') == 2
    (work/'step.inc').write_text('count SET missing\n')
    assert 'step.inc:1:' in run('INCLUDE step.inc\n', ok=False)
    for bad, message in [
        ('n SET missing\n', 'previously defined'),
        ('n SET $\n', 'previously defined'),
        ('n SET 1/0\n', 'previously defined'),
        ('n EQU 1\nn SET 2\n', 'cannot redefine'),
        ('n: nop\nn SET 2\n', 'cannot redefine'),
        ('n SET 1\nn EQU 2\n', 'cannot become'),
        ('n SET 1\nn: nop\n', 'cannot become'),
        (' SET 1\n', 'requires a symbol'),
        ('n SET 1,2\n', 'one absolute'),
        (' db n\nn SET 1\n', 'undefined'),
    ]:
        error = run(bad, ok=False)
        assert message in error, (message, error)
print('SET snapshots, repeats/includes, register preservation, expression precedence and byte equivalence passed')
