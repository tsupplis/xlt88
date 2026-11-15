#!/usr/bin/env python3
"""Macro expansion agrees with explicit source, with hygienic LOCAL labels."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
exe = Path(os.environ.get('XLT88', root / 'xlt88'))
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    source, output, state = work/'main.mac', work/'out.asm', work/'names.sym'

    def run(*args, ok=True):
        result = subprocess.run([str(exe), '--dialect=m80', *args, str(source), str(output)], capture_output=True)
        assert (result.returncode == 0) == ok, result.stderr.decode()
        return result.stderr.decode()

    def binary():
        dest = work/'out.bin'
        subprocess.run(['nasm', '-Werror', '-f', 'bin', str(output), '-o', str(dest)], check=True)
        return dest.read_bytes()

    body = '''step MACRO reg,val
 LOCAL again,done
 mvi reg,val
again: dcr reg
 jnz again
 jmp done
done:
 ENDM
outer MACRO reg
 step reg,3
 ENDM
bytes MACRO text
 db text
 ENDM
choose MACRO value
 IF value
  mvi a,value
 ELSE
  mvi a,9
 ENDIF
 ENDM
empty MACRO optional
 db 1 optional
 ENDM
'''
    (work/'defs.inc').write_text(body)
    main = '''.8080
INCLUDE defs.inc
outer b
here: step c,2
 choose 0
 choose 7
 bytes <'a,b',0>
 empty
 jmp 0
 END
'''
    source.write_text(main)
    flat = work/'flat.asm'
    flat.write_text(''' mvi b,3
one: dcr b
 jnz one
 jmp two
two:
here:
 mvi c,2
three: dcr c
 jnz three
 jmp four
four:
 mvi a,9
 mvi a,7
 db 'a,b',0
 db 1
 jmp 0
 end
''')
    for target in ('msdos', 'cpm86'):
        run('--target='+target)
        actual = binary()
        subprocess.run([str(exe), '--target='+target, str(flat), str(output)], check=True)
        assert binary() == actual
    run('--emit=asm80', '--state-out='+str(state))
    normalized = output.read_text()
    assert '__m80_1' in normalized and '__m80_4' in normalized
    flat.write_text(normalized)
    subprocess.run([str(exe), str(flat), str(output)], check=True)
    run('--state-in='+str(state))
    (work/'defs.inc').write_text(body.replace('step reg,3', 'step reg,4'))
    assert 'stale' in run('--state-in='+str(state), ok=False)
    (work/'defs.inc').write_text('broken MACRO\n mystery\n ENDM\n')
    source.write_text('INCLUDE defs.inc\nbroken\n')
    assert 'defs.inc:2:' in run(ok=False)

    # Includes in a macro resolve against its definition's directory.
    (work/'sub').mkdir()
    (work/'sub'/'defs.inc').write_text('inside MACRO\n INCLUDE value.inc\n ENDM\n')
    (work/'sub'/'value.inc').write_text(' mvi a,6\n')
    source.write_text('INCLUDE sub/defs.inc\n inside\n end\n')
    run('--emit=asm80')
    assert 'a,6' in output.read_text()

    # Expansion still supplies consistent record IDs to native DOS FCB analysis.
    source.write_text('whole MACRO\n' + (root/'tests/copy.asm').read_text() + '\n ENDM\n whole\n')
    run('--target=msdos')
    binary()

    # Lexical substitution is token-based, case-insensitive and leaves strings
    # and ordinary comments untouched. Double-semicolon comments are discarded.
    source.write_text('''load MACRO v
 db 'v',V ; v stays comment
 db v ;; hidden
 ENDM
 load 65
 end
''')
    run('--emit=asm80')
    text = output.read_text()
    assert "'v',65" in text and 'v stays comment' in text and 'hidden' not in text

    # A skipped definition must not let its internal ELSE affect the caller.
    source.write_text('IF 0\nskipped MACRO\n ELSE\n ENDM\nELSE\n mvi a,1\nENDIF\n end\n')
    run('--emit=asm80')
    assert 'mvi' in output.read_text()

    failures = [
        ('bad MACRO x,x\n ENDM\n', 'duplicate macro parameter'),
        ('bad MACRO\n', 'missing ENDM'),
        ('bad MACRO x\n LOCAL x\n ENDM\n', 'conflicts with parameter'),
        ('bad MACRO\n LOCAL x,x\n ENDM\n', 'duplicate LOCAL'),
        ('bad MACRO\n nop\n LOCAL x\n ENDM\n', 'precede'),
        ('bad MACRO\n inner MACRO\n ENDM\n ENDM\n', 'nested macro definitions'),
        ('bad MACRO\n ENDM\nbad MACRO\n ENDM\n', 'duplicate macro'),
        ('bad MACRO x\n ENDM\n bad 1,2\n', 'too many'),
        ('bad MACRO x\n ENDM\n bad <1,2\n', 'unclosed'),
        ('bad MACRO x\n ENDM\n bad %unknown\n', 'previously defined'),
        ('bad MACRO\n bad\n ENDM\n bad\n', 'nesting exceeds'),
        ('mov MACRO\n ENDM\n', 'conflicts with an operation'),
        ('LOCAL x\n', 'outside'),
        ('ENDM\n', 'outside'),
        ('jmp __m80_1\n', 'reserved'),
        ('bad MACRO x\n db x,x\n ENDM\n bad <'+','.join(['1']*100)+'>\n', 'exceeds 254'),
    ]
    for text, message in failures:
        source.write_text(text)
        output.write_text('preserve')
        error = run(ok=False)
        assert message in error, (message, error)
        assert output.read_text() == 'preserve'
print('M80 macro arguments, nested calls, LOCAL hygiene, provenance and rejection checks passed')
