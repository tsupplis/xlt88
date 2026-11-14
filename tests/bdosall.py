"""Assemble every non-reserved CP/M 2.2 native entry; do not execute this fixture."""
from pathlib import Path
import tempfile
from toolrun import runner

functions = tuple(range(38)) + (40,)
source = ' org 100h\n jmp 0\n'
for function in functions:
    source += f'svc{function}: mvi c,{function}\n call 5\n ret\n'
source += ' end\n'
with tempfile.TemporaryDirectory() as directory:
    _, build, _ = runner(Path(directory), 'cpm86')
    for flavor in ('nasm', 'asm86', 'masm3', 'drasm86'):
        build('bdosall', source, flavor)
        print(flavor, '39/39 native entry points assemble; not a behavioral coverage claim', flush=True)
