#!/usr/bin/env python3
"""Compare ASM86, MASM 3 and RASM-86 with NASM using native DOS and CP/M execution."""
import os
from pathlib import Path
import subprocess
import tempfile
from toolrun import runner
root = Path(__file__).resolve().parent.parent
exe = Path(os.environ.get('XLT88', root/'xlt88'))
target = os.environ.get('XLT_TARGET', 'cpm86')
flavor_test = os.environ.get('XLT_EMIT', 'asm86')
assert target in ('msdos', 'cpm86') and flavor_test in ('asm86', 'masm3', 'drasm86')
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    command, build, execute = runner(work, target)
    for fixture, marker in [('examples/hello.asm',b'Hello, world'),
                             ('examples/tst8080.asm',b'CPU IS OPERATIONAL'),
                             ('tests/andflags.asm',b'AND FLAGS OK')]:
        source = (root/fixture).read_text()
        outputs=[]
        for name,flavor in [('asmtest',flavor_test),('nasmtest','nasm')]:
            build(name,source,flavor)
            outputs.append(execute(name))
        assert outputs[0] == outputs[1] and marker in outputs[0], outputs
    build('opcodes',(root/'tests/opcodes.asm').read_text(),flavor_test)

    # Reserve/alignment, forward equates, direct/indirect memory and label spelling.
    source = '''alias equ later
here equ $
 lxi h,cell
 mov a,m
 ora a
 jnz failed
 mvi a,42
 sta cell+1
 lda cell+1
 cpi 42
 jnz failed
 lhld pair
 shld cell
 lda cell
 cpi 34h
 jnz failed
 lxi h,buffer
 mov a,l
 ani 7
 jnz failed
spot: lxi h,$
 lxi d,spot
 mov a,h
 cmp d
 jnz failed
 mov a,l
 cmp e
 jnz failed
 lhld self+($-$)
 lxi d,self
 mov a,h
 cmp d
 jnz failed
 mov a,l
 cmp e
 jnz failed
 lxi d,alias
 mvi c,9
 call 5
 jmp 0
failed: lxi d,error
 mvi c,9
 call 5
 jmp 0
 ds 0
cell: ds 3
pair: dw 1234h
self: dw $
 align 8
buffer: ds 128
CPU: dw later EQ later,later NE later
foo_bar: db 1
foobar: db 2
name@1: db 3
later: db 'EMITTER OK$'
error: db 'EMITTER FAILED$'
 end
'''
    outputs=[]
    for name,flavor in [('asmtest',flavor_test),('nasmtest','nasm')]:
        build(name,source,flavor)
        outputs.append(execute(name))
    assert outputs[0] == outputs[1] and b'EMITTER OK' in outputs[0], outputs
    if flavor_test in ('masm3','drasm86'):
        raw = (work/('asmtest.asm' if flavor_test == 'masm3' else 'asmtest.a86')).read_bytes()
        assert b'\r\n' in raw and b'\n' not in raw.replace(b'\r\n', b'')
    assert 'slater' in (work/('asmtest.a86' if flavor_test in ('asm86','drasm86') else 'asmtest.asm')).read_text()

    # CP/M FCB behavior, including zero-filled storage and extent boundaries.
    for name,flavor in [('asmcopy',flavor_test),('nascopy','nasm')]:
        build(name,(root/'tests/copy.asm').read_text(),flavor)
        for records in (0,1,129,260):
            data=bytes((i*37+i//128)&255 for i in range(records*128))
            (work/'input.dat').write_bytes(data)
            (work/'output.dat').unlink(missing_ok=True)
            assert b'COPY OK' in execute(name,'input.dat','output.dat')
            assert (work/'output.dat').read_bytes() == data

    # Representation failures leave an existing output intact.
    (work/'bad.80').write_text('wide equ 100000\n dw wide\n end\n')
    (work/'bad.a86').write_text('preserve')
    result=subprocess.run([str(exe),'--target='+target,'--emit='+flavor_test,'bad.80','bad.a86'],cwd=work,capture_output=True)
    assert result.returncode and (b'ASM86' if flavor_test == 'asm86' else b'RASM-86' if flavor_test == 'drasm86' else b'MASM 3') in result.stderr
    assert (work/'bad.a86').read_text() == 'preserve'
print(f'{flavor_test}/{target}: zero-warning assembly, CPU/console/memory/FCB equivalence and atomic rejection passed')
