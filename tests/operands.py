"""CPU operand shape diagnostics after reader expansion and PSW case folding."""
from itertools import product
from pathlib import Path
import subprocess
import tempfile
from toolrun import exe, runner

bad = ("nop a", "ret 1", "mov m,m", "mov a,sp", "mov a",
       "mvi psw,1", "lxi psw,1", "inr sp", "add sp", "push sp",
       "pop a", "stax h", "ldax sp", "dad a", "inx psw", "jmp",
       "call 1,2", "rst", "out 1,2")

source = ''' org 100h
 lxi h,55d7h
 push h
 pop PsW
 push pSw
 pop d
 mov a,d
 cpi 55h
 jnz failed
 mov a,e
 cpi 0d7h
 jnz failed
 mvi a,','
 cpi 44
 jnz failed
 lxi d,good
 jmp print
failed: lxi d,error
print: mvi c,9
 call 5
 jmp 0
good: db 'OPERANDS OK$'
error: db 'FAILED$'
 end
'''

with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    for dialect, flavor in product(('asm80', 'm80', 'mac', 'rmac'), ('asm80', 'nasm')):
        for instruction in bad:
            (work/'bad.80').write_text(' '+instruction+'\n end\n')
            (work/'out.asm').write_text('preserve')
            result = subprocess.run([str(exe), '--dialect='+dialect, '--emit='+flavor,
                                     'bad.80', 'out.asm'], cwd=work, capture_output=True)
            # RMAC may reject the operand earlier during relocation checking.
            diagnostic = b'invalid 8080 instruction operands' in result.stderr
            diagnostic |= dialect == 'rmac' and b'RMAC relocation expression' in result.stderr
            assert result.returncode and diagnostic, result
            assert (work/'out.asm').read_text() == 'preserve'
    for target, flavor in product(('msdos', 'cpm86'), ('nasm', 'asm86', 'masm3', 'drasm86')):
        _, build, execute = runner(work, target)
        build('operands', source, flavor)
        output = execute('operands')
        assert b'OPERANDS OK' in output and b'FAILED' not in output, (target, flavor, output)
    print('Operand rejection: all four readers; mixed-case PSW: all eight target/emitter combinations passed')
