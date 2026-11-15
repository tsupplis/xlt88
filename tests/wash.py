"""WASH15 reference comparison and unchanged-source acceptance."""
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
from toolrun import runner, assembler, gencmd, root, exe

source = root / 'examples/wash15.asm'
reference = root / 'examples/ref/was15.a86'
hashes = {path: hashlib.sha256(path.read_bytes()).hexdigest() for path in (source, reference)}
report = ['# WASH15 acceptance', '',
          'The supplied reference is xlt86 output with deliberate porting patches.',
          'Both it and the original 8080 source are used unchanged.', '',
          f'Source SHA256: `{hashes[source]}`', f'Reference SHA256: `{hashes[reference]}`', '']
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    command, build, execute = runner(work, 'cpm86')
    (work / 'wasref.a86').write_bytes(reference.read_bytes())
    log = command([assembler, 'wasref.a86'])
    assert b'NUMBER OF ERRORS:   0' in log and b'ERROR NO:' not in log, log
    command([gencmd, 'wasref', '8080'])
    reference_size = (work / 'wasref.cmd').stat().st_size
    contents = {'alpha.dat': b'ALPHA CONTENT\r\n\x1a', 'beta.dat': b'BETA CONTENT\r\n\x1a'}
    for name, data in contents.items():
        (work / name).write_bytes(data)
    cases = [('missing', 'missing.zzz', b'X', b'Not Found'),
             ('exit', 'alpha.dat', b'X', b'ALPHA'),
             ('browse', '*.dat', b' X', b'BETA'),
             ('help', 'alpha.dat', b'HX', b'Command'),
             ('view', 'alpha.dat', b'VX', b'ALPHA CONTENT')]
    report += ['## Reference scenarios', '']
    baseline = {}
    for case, argument, keys, token in cases:
        output = execute('wasref', argument, input=keys, console=True)
        assert token in output, (case, output)
        baseline[case] = output
        assert all((work / name).read_bytes() == data for name, data in contents.items())
        report.append(f'- {case}: passed; fixture files unchanged.')
        print('WASH reference', case, 'passed', flush=True)
    report += ['', '## Unchanged-source translation', '',
               f'Reference CMD: {reference_size} bytes. Sizes include container/padding; they are not instruction counts.', '',
               '| Target | Result | CMD bytes | Size / reference |', '| --- | --- | ---: | ---: |']
    for flavor in ('nasm', 'asm86', 'masm3', 'drasm86'):
        build('wash', source.read_bytes(), flavor, dialect='m80')
        for case, argument, keys, token in cases:
            output = execute('wash', argument, input=keys, console=True)
            assert output == baseline[case], (flavor, case, output, baseline[case])
            assert all((work / name).read_bytes() == data for name, data in contents.items())
        size = (work / 'wash.cmd').stat().st_size
        report.append(f'| cpm86/{flavor} | all five reference transcripts match | {size} | {size / reference_size:.2f} |')
        print('WASH cpm86', flavor, 'all reference transcripts match', flush=True)
    result = subprocess.run([str(exe), '--dialect=m80', '--target=msdos',
                             str(source), str(work / 'doswash.asm')], capture_output=True)
    diagnostics = result.stderr.decode()
    assert result.returncode and 'FCB' in diagnostics, diagnostics
    report.append('| msdos | blocked by FCB representation; no execution claimed | — | — |')
    report += ['', '```text', diagnostics.rstrip(), '```', '',
               '## Remaining porting work', '',
               '- Interrupt policy: the original disables interrupts; the patched reference explicitly enables them after stack setup. Original DI is preserved. Emulator transcript passes do not validate hardware interrupt policy.',
               '- Selector dispatch uses direct-store candidates with checks on every arm. Unmatched CP/M-86 selectors use warned native pass-through; no memory alias proof is claimed.',
               '- Native reset/select/DMA/search implement reset without simulated drive state. Reset state tests live in allreset.py; protected-drive state still needs emulator support.',
               '- Inline strings and code-label data access are exercised by these transcripts. Seven additional file-operation/cancellation comparisons are in washfile.md; successful copy has separate reference limitations. Devices and drive-switch workflows remain.',
               '- DOS needs directory-result and explicit FCB-field translation.',
               '', 'No program-name exception or source patch has been added.']
assert all(hashlib.sha256(path.read_bytes()).hexdigest() == digest for path, digest in hashes.items())
if len(sys.argv) > 1:
    Path(sys.argv[1]).write_text('\n'.join(report) + '\n')
