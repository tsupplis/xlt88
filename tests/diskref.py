"""DISK7 reference acceptance and honest unchanged-source blocker inventory."""
import hashlib
import re
from pathlib import Path
import subprocess
import sys
import tempfile
from toolrun import runner, root, exe, assembler, gencmd, env, cmd_span
from xltref import fresh_compare


def pointer_port(text, flavor):
    """Reviewed DISK7 ES consumers, kept out of production translation.

    These are the five segment overrides already present in the supplied
    reference. They do not resolve the source's allocation/CP/M 3 assumptions.
    """
    label = (lambda name: '$'+name.upper()) if flavor == 'nasm' else (lambda name: 's'+name.lower())
    first = text.index(label('frestor')+':')
    last = text.index(label('free20')+':', first)
    block = text[first:last]
    assert block.count('[bx]') == 6, block  # Four DPB reads, two CP/M 3 DMA accesses.
    block = block.replace('[bx]', '[es:bx]' if flavor == 'nasm' else 'es:[bx]', 4)
    if flavor in ('asm86', 'drasm86'):
        block = block.replace('byte ptr es:[bx]', 'es:byte ptr [bx]')
    text = text[:first]+block+text[last:]
    first = text.index(label('gspbyt')+':')
    last = text.index(label('gsplup')+':', first)
    block = text[first:last]
    assert block.count('[si]') == 1, block
    block = block.replace('[si]', '[es:si]' if flavor == 'nasm' else 'es:[si]')
    return text[:first]+block+text[last:]

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
    destination = work / 'dest'
    destination.mkdir()
    env['EMU2_DRIVE_A'] = str(files)
    env['EMU2_DRIVE_B'] = str(destination)
    env['EMU2_DRIVE_C'] = str(work)
    env['EMU2_DEFAULT_DRIVE'] = 'C:'
    command, build, execute = runner(work, 'cpm86')
    (work / 'diskref.a86').write_bytes(reference.read_bytes())
    log = command([assembler, 'diskref.a86'])
    assert b'NUMBER OF ERRORS:   0' in log and b'ERROR NO:' not in log, log
    command([gencmd, 'diskref', '8080'])
    reference_span = cmd_span(work/'diskref.cmd')
    env['EMU2_DEFAULT_DRIVE'] = 'A:'
    report += [f'Reference CMD size: {(work / "diskref.cmd").stat().st_size} bytes (including container/padding).', '',
               '## Reference scenarios', '']
    contents = {'alpha.dat': b'ALPHA CONTENT\r\n\x1a', 'beta.dat': b'BETA CONTENT\r\n\x1a'}
    for name, data in contents.items():
        (files / name).write_bytes(data)
    baseline = {}
    cases = [('exit', b'X', b'ALPHA'), ('browse', b' X', b'BETA'),
             ('length', b'LX', b'ALPHA'), ('view', b'VX', b'ALPHA CONTENT')]
    for case, keys, token in cases:
        output = execute('diskref', '*.dat', input=keys, console=True)
        baseline[case] = output
        assert token in output and b'bytes free' in output, (case, output)
        if case == 'length':
            assert re.search(rb': L\x08[1-9][0-9]*k\r\n', output), output
        assert {p.name: p.read_bytes() for p in files.iterdir()} == contents
        # Repeatability is necessary before using this transcript as an oracle.
        assert execute('diskref', '*.dat', input=keys, console=True) == output, case
        report.append(f'- {case}: passed twice with identical transcripts; fixture files unchanged.')
        print('DISK7 reference', case, 'repeatable', flush=True)
    env['EMU2_DEFAULT_DRIVE'] = 'C:'
    fresh, same = fresh_compare(work, source, 'mac', 'disk', report,
                               native_port=lambda text: pointer_port(text, 'asm86'))
    env['EMU2_DEFAULT_DRIVE'] = 'A:'
    for case, keys, token in cases:
        for name in (fresh, same):
            assert execute(name, '*.dat', input=keys, console=True) == baseline[case], (name, case)
    for name in ('diskref', fresh, same):
        for path in destination.iterdir():
            path.unlink()
        output = execute(name, 'alpha.dat', input=b'CB:\nX', timeout=15)
        snapshot = {p.name.lower(): p.read_bytes() for p in destination.iterdir()}
        if name == 'diskref':
            copy_reference = output, snapshot
        else:
            assert (output, snapshot) == copy_reference, name
    report += ['Fresh XLT86 and identical-input xlt88 explicit ports match four reference transcripts and cross-drive copy.', '']
    report += ['', f'Reference loaded span: {reference_span} bytes (includes data/padding).', '',
               '## Raw translation and explicit ES port', '',
               '| Emitter | Result | Raw loaded span |', '| --- | --- | ---: |']
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
        env['EMU2_DEFAULT_DRIVE'] = 'A:'
        raw = execute('diskout', '*.dat', input=b'X', console=True, timeout=15)
        assert b'ALPHA' in raw
        raw_equal = raw == baseline['exit']
        env['EMU2_DEFAULT_DRIVE'] = 'C:'
        build('diskport', source.read_bytes(), flavor, dialect='mac',
              patch=lambda text: pointer_port(text, flavor))
        env['EMU2_DEFAULT_DRIVE'] = 'A:'
        for case, keys, token in cases:
            output = execute('diskport', '*.dat', input=keys, console=True, timeout=15)
            assert output == baseline[case], (flavor, case, output, baseline[case])
            assert {p.name: p.read_bytes() for p in files.iterdir()} == contents
        # Compare a real cross-drive copy, including CP/M record padding.
        copied = []
        for program in ('diskref', 'diskport'):
            for path in destination.iterdir():
                path.unlink()
            output = execute(program, 'alpha.dat', input=b'CB:\nX', timeout=15)
            snapshot = {p.name.lower(): p.read_bytes() for p in destination.iterdir()}
            assert set(snapshot) == {'alpha.dat'} and snapshot['alpha.dat'].startswith(contents['alpha.dat']), output
            copied.append((output, snapshot))
            assert {p.name: p.read_bytes() for p in files.iterdir()} == contents
        assert copied[0] == copied[1], (flavor, copied)
        outcome = f'raw startup transcript equal: {raw_equal}; explicit ES port: four transcripts and copy match'
        report.append(f'| {flavor} | {outcome} | {cmd_span(work/"diskout.cmd")} |')
        print('DISK7', flavor, outcome, flush=True)
        detail = diagnostics
    report += ['', 'Representative diagnostics:', '```text', detail.rstrip(), '```']

assert all(hashlib.sha256(p.read_bytes()).hexdigest() == digest for p, digest in hashes.items())
if len(sys.argv) > 1:
    Path(sys.argv[1]).write_text('\n'.join(report) + '\n')
