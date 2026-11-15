"""Measure unchanged VTL-2 against the supplied direct xlt86 reference."""
import hashlib
import re
import sys
import tempfile
from pathlib import Path
from toolrun import runner, assembler, gencmd

root = Path(__file__).resolve().parents[1]
source = root/'examples/vtl2.asm'
reference = root/'examples/ref/vtl2.a86'
script = b'?=123+456\rA=7\r?=A*6\r?=1234-567\r?=255*255\r?=65535/255\r?=65535+1\r>=\r'
expected = (b'VTL2 Interpreter 1.0', b'\r\n579\r\n', b'\r\n42\r\n', b'\r\n667\r\n', b'\r\n65025\r\n', b'\r\n257\r\n', b'\r\n0\r\n')
hashes = {p:hashlib.sha256(p.read_bytes()).hexdigest() for p in (source,reference)}
report = ['# VTL-2 comparison benchmark','',
          'The source and direct xlt86 reference are used unchanged. No source-specific patches are applied.',
          '',f'Source SHA256: `{hashes[source]}`',f'Reference SHA256: `{hashes[reference]}`','']
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    command, _, execute = runner(work,'cpm86')
    (work/'baseline.a86').write_bytes(reference.read_bytes())
    log = command([assembler,'baseline.a86'])
    assert b'NUMBER OF ERRORS:   0' in log and b'ERROR NO:' not in log, log
    listing = (work/'baseline.lst').read_text(errors='replace')
    boundary = re.search(r'^\s*([0-9A-F]+)\s+L_37\s+EQU\s+\$',listing,re.M)
    command([gencmd,'baseline','8080'])
    output = execute('baseline',input=script,console=True)
    assert all(token in output for token in expected), output
    if boundary:
        report += [f'Reference end of code: **{boundary[1]}h**; fixed variable origin: **0800h**.','']
    report += ['## xlt88 results','', '| Target | Emitter | Result |', '| --- | --- | --- |']
    for target in ('cpm86','msdos'):
        _, build, run = runner(work,target)
        for flavor in ('nasm','asm86','masm3','drasm86'):
            build('vtl',source.read_bytes(),flavor)
            result = run('vtl',input=script,console=True)
            assert all(token in result for token in expected), (target,flavor,result)
            report.append(f'| {target} | {flavor} | reference result-token checks passed |')
            print(target,flavor,'reference result-token checks passed',flush=True)

assert all(hashlib.sha256(p.read_bytes()).hexdigest() == digest for p,digest in hashes.items())
if len(sys.argv) > 1:
    Path(sys.argv[1]).write_text('\n'.join(report)+'\n')
print('Reference acceptance passed; all target results recorded explicitly',flush=True)
