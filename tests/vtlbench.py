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
regressions = []
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
    report += ['## Reference execution','',
               'ASM86 1.1N2 assembly and GENCMD combined-image packaging succeeded.',
               'Execution uses emu2 in 8088 mode: startup, addition (579), variable assignment/multiplication (42), subtraction (667), wide multiplication (65025), division (257), 16-bit wrap (0), and clean exit passed.','',
               '```text',output.decode(errors='replace').replace('\r','').rstrip(),'```','']
    if boundary:
        report += [f'Reference end of code: **{boundary[1]}h**; fixed variable origin: **0800h**.','']
    report += ['## xlt88 results','', '| Target | Emitter | Result |', '| --- | --- | --- |']
    details = []
    for target in ('cpm86','msdos'):
        _, build, run = runner(work,target)
        for flavor in ('nasm','asm86','masm3','drasm86'):
            try:
                build('vtl',source.read_bytes(),flavor)
            except AssertionError as error:
                regressions.append((target, flavor))
                report.append(f'| {target} | {flavor} | blocked before execution |')
                details += ['',f'### {target}/{flavor}','','```text',str(error)[:1800],'```']
                print(target,flavor,'blocked before execution',flush=True)
            else:
                result = run('vtl',input=script,console=True)
                assert all(token in result for token in expected), (target,flavor,result)
                report.append(f'| {target} | {flavor} | reference transcript checks passed |')
                print(target,flavor,'reference transcript checks passed',flush=True)
    report += ['', 'A blocked result is not an execution pass. The table records each current result independently. Both targets are required acceptance gates; arbitrary memory uses native target semantics.',
               'The fixed origin is required by eight-bit variable indexing. The translated source and variable location remain unchanged; only proven general code-size optimisations are applied.'] + details
assert all(hashlib.sha256(p.read_bytes()).hexdigest() == digest for p,digest in hashes.items())
if len(sys.argv) > 1:
    Path(sys.argv[1]).write_text('\n'.join(report)+'\n')
assert not regressions, ('Established VTL-2 execution targets regressed', regressions)
print('Reference acceptance passed; all target results recorded explicitly',flush=True)
