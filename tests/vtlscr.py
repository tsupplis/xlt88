"""Unmodified VTL-2 script input, checked first against the xlt86 reference."""
from pathlib import Path
import tempfile
from toolrun import runner, assembler, gencmd, root

# Crossing 128-byte records and CR/LF handling are independent of interpreter
# input from the console. Every script must return to the console at EOF.
program = b'10 A=7\r\n20 ?=A*6\r\n30 #=0\r\n#=10\r\n'
cases = (
    ('empty', b'', ()),
    ('stored', program + b'\x1a', (b'\r\n42\r\n',)),
    ('records', b'\r\n' * 4 + b'?=123+456\r\n' * 30 + b'\x1a', (b'\r\n579\r\n',)),
    ('marker', b'?=1\r\n\x1a?=2\r\n', (b'\r\n1\r\n',)),
    ('missing', None, ()),
)
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    command, build, execute = runner(work, 'cpm86')
    (work/'baseline.a86').write_bytes((root/'examples/ref/vtl2.a86').read_bytes())
    command([assembler, 'baseline.a86'])
    command([gencmd, 'baseline', '8080'])
    baseline = {}

    def check(name, case, data, expected):
        path = work/(case + '.vtl')
        if data is None:
            path.unlink(missing_ok=True)
        else:
            path.write_bytes(data)
        output = execute(name, path.name, input=b'?=777\r>=\r', console=True)
        assert b'\r\n777\r\n' in output, (name, case, output)
        assert all(token in output for token in expected), (name, case, output)
        if case == 'marker':
            assert b'\r\n2\r\n' not in output, (name, case, output)
        if case == 'records':
            assert output.count(b'\r\n579\r\n') == 30, (name, output)
        if data is not None:
            assert path.read_bytes() == data
        return output

    for case, data, expected in cases:
        baseline[case] = check('baseline', case, data, expected)
    for target in ('cpm86', 'msdos'):
        _, build, execute = runner(work, target)
        for flavor in ('nasm', 'asm86', 'masm3', 'drasm86'):
            build('vtlscr', (root/'examples/vtl2.asm').read_bytes(), flavor)
            for case, data, expected in cases:
                output = check('vtlscr', case, data, expected)
                # CP/M warm boot prints one final CR/LF; DOS INT 20h does not.
                comparable = output + b'\r\n' if target == 'msdos' else output
                assert comparable == baseline[case], (target, flavor, case, output, baseline[case])
            print(target, flavor, 'stored programs, multi-record scripts, empty/missing files and EOF match xlt86', flush=True)
