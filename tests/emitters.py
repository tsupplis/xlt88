#!/usr/bin/env python3
"""Compare ASM86 and MASM 3 with NASM using native DOS and CP/M execution."""
import os
import re
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parent.parent
exe = Path(os.environ.get('XLT88', root/'xlt88'))
assembler = os.environ.get('ASM86', 'cpm86_asm86')
gencmd = os.environ.get('GENCMD', 'cpm86_gencmd')
emu = os.environ.get('EMU2', 'emu2')
env = dict(os.environ, EMU2_CPM_VER='2.2', EMU2_DOSVER='1.0')
target = os.environ.get('XLT_TARGET', 'cpm86')
flavor_test = os.environ.get('XLT_EMIT', 'asm86')
assert target in ('msdos', 'cpm86') and flavor_test in ('asm86', 'masm3')
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    def command(args, execution=False, codes=(0,)):
        result = subprocess.run(args, cwd=work, env=env if execution else dict(env, EMU2_DOSVER="3.3"), capture_output=True, timeout=60)
        assert result.returncode in codes, (args, result.stdout, result.stderr)
        return result.stdout
    def build(name, text, flavor):
        source = work/(name+'.80')
        source.write_text(text)
        suffix = '.a86' if flavor == 'asm86' else '.asm'
        command([str(exe),'--target='+target,'--emit='+flavor,str(source),name+suffix])
        if flavor == 'asm86':
            log = command([assembler,name+'.a86'])
            # Ignored syntax is still a regression, even if the wrapper accepts it.
            assert b'ERROR NO:' not in log and b'NUMBER OF ERRORS:   0' in log, log
            command([gencmd,name,'8080'])
            if target == 'msdos':
                # Combined CMD: one code group, 128-byte header, 100h prefix.
                image = (work/(name+'.cmd')).read_bytes()
                assert image[0] == 1 and image[9] == 0 and image[3:5] == b'\0\0'
                size = int.from_bytes(image[1:3], 'little') * 16
                assert 256 < size <= len(image)-128
                assert image[128:384] == bytes(256)
                (work/(name+'.com')).write_bytes(image[384:128+size])
        elif flavor == 'masm3':
            log = command([os.environ.get('MASM3','pcdev_masm3'),name+'.asm,'+name+'.obj;'])
            assert re.search(rb'Errors\s+Errors\s+0\s+0',log) and b'E r r o r' not in log, log
            log = command([os.environ.get('LINK2','pcdev_link2'),name+'.obj,'+name+'.exe;'], codes=(0,1))
            assert b'error' not in log.lower(), log
            warnings = [line.strip() for line in log.splitlines() if b'warning' in line.lower()]
            assert warnings == [b'Warning: no STACK segment'], log
            command([os.environ.get('EXE2BIN','pcdev_exe2bin'),name+'.exe',name+'.com'])
            assert (work/(name+'.com')).stat().st_size > 0
            if target == 'cpm86':
                command(['bin2cmd',name+'.com',name+'.cmd'])
        else:
            command(['nasm','-Werror','-f','bin',name+'.asm','-o',name+'.com'])
            if target == 'cpm86':
                command(['bin2cmd',name+'.com',name+'.cmd'])
    def execute(name, *args):
        return command([emu,name+('.cmd' if target == 'cpm86' else '.com'),*args], execution=True)
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
    assert 'slater' in (work/('asmtest.a86' if flavor_test == 'asm86' else 'asmtest.asm')).read_text()

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
    assert result.returncode and (b'ASM86' if flavor_test == 'asm86' else b'MASM 3') in result.stderr
    assert (work/'bad.a86').read_text() == 'preserve'
print(f'{flavor_test}/{target}: zero-warning assembly, CPU/console/memory/FCB equivalence and atomic rejection passed')
