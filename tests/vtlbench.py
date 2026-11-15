"""Measure unchanged VTL-2 against the supplied direct xlt86 reference."""
import hashlib
import os
import re
import sys
import tempfile
from pathlib import Path
from toolrun import runner, assembler, gencmd, exe, hex_image, cmd_span
from xltref import fresh_compare, compare_psw

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
    # Build the binary from unchanged source with a real 8080 assembler.
    (work/'input.asm').write_bytes(source.read_bytes())
    command([os.environ.get('ASM80', 'cpm_asm80'), 'input'])
    binary = hex_image(work/'input.hex')
    (work/'input.com').write_bytes(binary)
    command([str(exe), '--dialect=com', '--emit=asm80', 'input.com', 'recover.asm'], codes=(2,))
    command([os.environ.get('ASM80', 'cpm_asm80'), 'recover'])
    assert hex_image(work/'recover.hex') == binary
    # Review only classification and the page-aligned layout from the source.
    # Pointer sites must come from the analyzer, not this benchmark.
    mapping = work/'recover.map'
    layout = mapping.read_text()
    assert re.findall(r'^unknown .*$', layout, re.M) == [
        'unknown 0406 0411', 'unknown 04b5 04cf', 'unknown 05e8 0885']
    layout = re.sub(r'^unknown ', 'data ', layout, flags=re.M)
    layout = layout.replace('data 05e8 0885', 'gap 05e8 0800\ndata 0800 0885')
    mapping.write_text(layout)
    report += ['COM input is assembled from unchanged source and round-trips byte-for-byte.',
               'Its generated map is reviewed for three data ranges and the fixed 0800h layout; pointer candidates are automatic.', '']
    (work/'baseline.a86').write_bytes(reference.read_bytes())
    log = command([assembler,'baseline.a86'])
    assert b'NUMBER OF ERRORS:   0' in log and b'ERROR NO:' not in log, log
    listing = (work/'baseline.lst').read_text(errors='replace')
    boundary = re.search(r'^\s*([0-9A-F]+)\s+L_37\s+EQU\s+\$',listing,re.M)
    command([gencmd,'baseline','8080'])
    report += [f'Reference loaded span: {cmd_span(work/"baseline.cmd")} bytes (includes data/padding).', '']
    output = execute('baseline',input=script,console=True)
    assert all(token in output for token in expected), output
    if boundary:
        report += [f'Reference end of code: **{boundary[1]}h**; fixed variable origin: **0800h**.','']
    fresh, same = fresh_compare(work, source, 'asm80', 'vtl', report)
    assert execute(fresh, input=script, console=True) == execute(same, input=script, console=True)
    compare_psw(work, report)
    report += ['## xlt88 results','', '| Target | Emitter | Result | Loaded span |', '| --- | --- | --- | ---: |']
    for target in ('cpm86','msdos'):
        _, build, run = runner(work,target)
        for flavor in ('nasm','asm86','masm3','drasm86'):
            build('vtl',source.read_bytes(),flavor)
            span = cmd_span(work/'vtl.cmd') if target == 'cpm86' else '—'
            result = run('vtl',input=script,console=True)
            assert all(token in result for token in expected), (target,flavor,result)
            build('vtlbin',binary,flavor,dialect='com',com_map=mapping)
            assert run('vtlbin',input=script,console=True) == result, (target,flavor,'COM console mismatch')
            # Stored programs, multiple FCB records and returning to console.
            program = b'10 A=7\r\n20 ?=A*6\r\n30 #=0\r\n#=10\r\n' + b'?=123+456\r\n'*15 + b'\x1a'
            path = work/'program.vtl'
            path.write_bytes(program)
            source_result = run('vtl',path.name,input=b'?=777\r>=\r',console=True)
            binary_result = run('vtlbin',path.name,input=b'?=777\r>=\r',console=True)
            assert source_result == binary_result, (target,flavor,'COM file mismatch')
            assert b'\r\n42\r\n' in binary_result and b'\r\n777\r\n' in binary_result
            assert binary_result.count(b'\r\n579\r\n') == 15 and path.read_bytes() == program
            if target == 'cpm86' and flavor == 'asm86':
                fresh_result = run(fresh, path.name, input=b'?=777\r>=\r', console=True)
                assert fresh_result == run(same, path.name, input=b'?=777\r>=\r', console=True)
                assert fresh_result == source_result
            report.append(f'| {target} | {flavor} | reference tokens pass; COM/source console and script output identical | {span} |')
            print(target,flavor,'reference checks and COM/source console + file equivalence passed',flush=True)

assert all(hashlib.sha256(p.read_bytes()).hexdigest() == digest for p,digest in hashes.items())
if len(sys.argv) > 1:
    Path(sys.argv[1]).write_text('\n'.join(report)+'\n')
print('Reference acceptance passed; all target results recorded explicitly',flush=True)
