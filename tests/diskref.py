"""DISK7 reference acceptance and honest unchanged-source blocker inventory."""
import hashlib
import re
from pathlib import Path
import subprocess
import sys
import tempfile
from toolrun import runner, root, exe, assembler, gencmd, env

source = root / 'examples/disk7.mac'
reference = root / 'examples/ref/disk7.a86'
hashes = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in (source, reference)}
report = ['# DISK7 reference acceptance', '',
          'The supplied ASM86 reference includes manual porting patches. Both inputs remain unchanged.', '',
          f'Source SHA256: `{hashes[source]}`', f'Reference SHA256: `{hashes[reference]}`', '']
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    files = work / 'files'
    files.mkdir()
    env['EMU2_DRIVE_A'] = str(files)
    env['EMU2_DRIVE_C'] = str(work)
    env['EMU2_DEFAULT_DRIVE'] = 'C:'
    command, build, execute = runner(work, 'cpm86')
    (work / 'diskref.a86').write_bytes(reference.read_bytes())
    log = command([assembler, 'diskref.a86'])
    assert b'NUMBER OF ERRORS:   0' in log and b'ERROR NO:' not in log, log
    command([gencmd, 'diskref', '8080'])
    env['EMU2_DEFAULT_DRIVE'] = 'A:'
    report += [f'Reference CMD size: {(work / "diskref.cmd").stat().st_size} bytes (including container/padding).', '',
               '## Reference scenarios', '']
    contents = {'alpha.dat': b'ALPHA CONTENT\r\n\x1a', 'beta.dat': b'BETA CONTENT\r\n\x1a'}
    for name, data in contents.items():
        (files / name).write_bytes(data)
    for case, keys, token in [('exit', b'X', b'ALPHA'), ('browse', b' X', b'BETA'),
                              ('length', b'LX', b'ALPHA'), ('view', b'VX', b'ALPHA CONTENT')]:
        output = execute('diskref', '*.dat', input=keys, console=True)
        assert token in output and b'bytes free' in output, (case, output)
        if case == 'length':
            assert re.search(rb': L\x08[1-9][0-9]*k\r\n', output), output
        assert {p.name: p.read_bytes() for p in files.iterdir()} == contents
        # Repeatability is necessary before using this transcript as an oracle.
        assert execute('diskref', '*.dat', input=keys, console=True) == output, case
        report.append(f'- {case}: passed twice with identical transcripts; fixture files unchanged.')
        print('DISK7 reference', case, 'repeatable', flush=True)
    report += ['', '## Unchanged-source results', '', '| Emitter | Result |', '| --- | --- |']
    detail = ''
    for flavor in ('nasm', 'asm86', 'masm3', 'drasm86'):
        result = subprocess.run([str(exe), '--dialect=mac', '--target=cpm86', '--emit='+flavor,
                                 str(source), str(work/'diskout.asm')], capture_output=True)
        diagnostics = result.stderr.decode()
        assert result.returncode == 0 and 'native BDOS pass-through' in diagnostics, diagnostics
        assert 'BDOS 27 returns native ES:BX' in diagnostics and 'BDOS 31 returns native ES:BX' in diagnostics
        env['EMU2_DEFAULT_DRIVE'] = 'C:'
        # All four assemblers now pass. A timeout/rejection is a regression,
        # not a report-only outcome (ASM86's expression stack was repaired).
        build('diskout', source.read_bytes(), flavor, dialect='mac')
        outcome = 'translates and assembles; execution requires explicit port review'
        report.append(f'| {flavor} | {outcome} |')
        print('DISK7', flavor, outcome, flush=True)
        detail = diagnostics
    report += ['', 'Representative diagnostics:', '', '```text', detail.rstrip(), '```', '',
               'Generated DISK7 is not executed: pointer segments, BIOS access and exit/stack assumptions still require review. No transcript equivalence is claimed.',
               'Function 46 now passes through natively. The version-dependent branch is retained; its native availability and semantics remain the responsibility of the port author.',
               'The reference ES overrides remain deliberate target adjustments, not application-specific translator rules.']
assert all(hashlib.sha256(p.read_bytes()).hexdigest() == digest for p, digest in hashes.items())
Path(sys.argv[1] if len(sys.argv) > 1 else 'diskref.md').write_text('\n'.join(report) + '\n')
