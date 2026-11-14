#!/usr/bin/env python3
"""M80 preprocessing: selected code, provenance and transitive input safety."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parent.parent
exe = Path(os.environ.get('XLT88', root / 'xlt88'))
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    sub = work / 'sub'
    sub.mkdir()
    source, output, state = work/'main.mac', work/'out.asm', work/'names.sym'
    main = '''.8080
flag EQU 1
IF flag
 INCLUDE "sub/one.inc"
ELSE
 INCLUDE missing.inc
 bad MACRO
 END
ENDIF
IF 0
 IF1
 ENDM
 ENDIF
ENDIF
IFDEF count
 IFE count-2
  mvi a,7
 ELSE
  undefined_opcode
 ENDIF
ENDIF
IFNDEF absent
 jmp 0
ENDIF
END
'''
    source.write_text(main)
    (sub/'one.inc').write_text("count EQU 2\nINCLUDE 'two.inc'\n")
    (sub/'two.inc').write_text('IFT 1\n mvi b,3\nENDIF\n')

    def run(*args, ok=True, dest=output):
        result = subprocess.run([str(exe), '--dialect=m80', *args, str(source), str(dest)], capture_output=True)
        assert (result.returncode == 0) == ok, result.stderr.decode()
        return result.stderr.decode()

    def binary(path):
        out = work/'out.bin'
        subprocess.run(['nasm', '-Werror', '-f', 'bin', str(path), '-o', str(out)], check=True)
        return out.read_bytes()

    for target in ('msdos', 'cpm86'):
        run('--target='+target)
        actual = binary(output)
        flat = work/'flat.asm'
        flat.write_text('count equ 2\n mvi b,3\n mvi a,7\n jmp 0\n end\n')
        subprocess.run([str(exe), '--target='+target, str(flat), str(output)], check=True)
        assert binary(output) == actual
    run('--emit=8080', '--state-out='+str(state))
    normalized = output.read_text()
    assert 'missing.inc' not in normalized and 'undefined_opcode' not in normalized
    flat = work/'flat.asm'
    flat.write_text(normalized)
    subprocess.run([str(exe), str(flat), str(output)], check=True)
    run('--state-in='+str(state))
    (sub/'two.inc').write_text('mvi b,4\n')
    assert 'stale' in run('--state-in='+str(state), ok=False)
    (sub/'two.inc').write_text('; origin\nbroken EQU 1/0\n')
    error = run(ok=False)
    assert 'two.inc:2:' in error and 'arithmetic' in error
    (sub/'two.inc').write_text('; origin\n mystery\n')
    assert 'two.inc:2:' in run(ok=False)
    (sub/'two.inc').write_text('INCLUDE ../main.mac\n')
    assert 'cycle' in run(ok=False)
    (sub/'two.inc').write_text('mvi b,3\n')
    saved = (sub/'one.inc').read_bytes()
    assert 'overwrite an included source' in run(dest=sub/'one.inc', ok=False)
    assert 'overwrite an included source' in run('--state-out='+str(sub/'one.inc'), ok=False)
    assert (sub/'one.inc').read_bytes() == saved
    for text, message in [
        ('IF unknown\nENDIF\n', 'previously defined'),
        ('IF 1\n', 'missing ENDIF'),
        ('IF 0\nELSE\nELSE\nENDIF\n', 'duplicate ELSE'),
        ('ENDIF\n', 'unmatched'),
        ('INCLUDE missing.inc\n', 'cannot open'),
        ('IFDEF 123\nENDIF\n', 'symbol name'),
        ('IF2\nENDIF\n', 'not implemented'),
        ('IF 1\nEND\nENDIF\n', 'before matching'),
    ]:
        source.write_text(text)
        output.write_text('preserve')
        assert message in run(ok=False)
        assert output.read_text() == 'preserve'
    # A complete included CP/M program retains valid expanded-record IDs
    # for the FCB pass, including helper labels and DS storage definitions.
    (sub/'copy.inc').write_text((root/'tests/copy.asm').read_text())
    source.write_text('.8080\nINCLUDE sub/copy.inc\n')
    run('--target=msdos')
    binary(output)
print('M80 nested conditions/includes, provenance, state invalidation and source protection passed')
