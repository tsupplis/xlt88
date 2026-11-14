"""Exhaustive accumulator results/PSW for AND masks with constant AC."""
from pathlib import Path
import tempfile
from toolrun import runner

with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    for target in ('msdos', 'cpm86'):
        _, build, execute = runner(work, target)
        for flavor in ('nasm', 'asm86', 'masm3', 'drasm86'):
            for mask in (8, 15, 127, 255):
                for start in range(0, 256, 64):
                    lines = [' org 100h']
                    for a in range(start, start + 64):
                        result = a & mask
                        psw = 18 | (result & 128) | (64 if result == 0 else 0)
                        psw |= 4 if result.bit_count() % 2 == 0 else 0
                        lines += [f' mvi a,{a}', ' stc', f' ani {mask}', ' push psw', ' pop b',
                                  f' cpi {result}', ' jnz failed', ' mov a,c', f' cpi {psw}', ' jnz failed']
                    lines += [' lxi d,good', ' jmp print', 'failed: lxi d,bad', 'print: mvi c,9',
                              ' call 5', ' jmp 0', "good: db 'AND OK$'", "bad: db 'AND FAILED$'", ' end']
                    build('andlive', '\n'.join(lines), flavor)
                    output = execute('andlive')
                    assert b'AND OK' in output and b'FAILED' not in output, (target, flavor, mask, start, output)
            print(target, flavor, '1024 AND result/PSW cases passed', flush=True)
