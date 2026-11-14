"""Native DOS buffered input: caller capacity, count, contents and CR guards."""
import tempfile
from pathlib import Path
from toolrun import runner

with tempfile.TemporaryDirectory() as directory:
    _, build, execute = runner(Path(directory), 'msdos')
    for flavor in ('nasm', 'asm86', 'masm3', 'drasm86'):
        for capacity, inputs in ((0, [b'']), (1, [b'']), (1, [b'A']),
                                 (4, [b'AB']), (4, [b'ABCD']),
                                 (4, [b'AB', b'X']), (254, [b'A' * 254]), (255, [b''])):
            expected = [capacity, 222] + [165] * capacity + [91, 92]
            for data in inputs:
                expected[1] = len(data)
                expected[2:2 + len(data)] = data
            source = ' mvi b,2\n' if len(inputs) == 2 else ''
            source += 'again: lxi d,buffer\n mvi c,10\n call 5\n'
            if len(inputs) == 2:
                source += ' dcr b\n jnz again\n'
            for offset, byte in enumerate(expected):
                source += f' lda buffer+{offset}\n cpi {byte}\n jnz bad\n'
            source += " lxi d,ok\n mvi c,9\n call 5\n jmp 0\nbad: jmp 0\nok: db 'BUFFER OK$'\nbuffer:\n"
            initial = [capacity, 222] + [165] * capacity + [91, 92]
            for i in range(0, len(initial), 16):
                source += ' db ' + ','.join(map(str, initial[i:i + 16])) + '\n'
            source += ' end\n'
            build('conbuf', source, flavor)
            output = execute('conbuf', input=b'\r'.join(inputs) + b'\r')
            if capacity == 255:
                assert b'capacity 255 needs a native port' in output and b'BUFFER OK' not in output
            else:
                assert b'BUFFER OK' in output, (flavor, capacity, inputs, output)
        print(flavor, 'buffer capacities/counts, exact writes, CR guards and repeated reads passed', flush=True)
