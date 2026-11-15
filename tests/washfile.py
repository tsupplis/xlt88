"""Unchanged WASH/reference file operations; copy limitations stay explicit."""
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
from toolrun import runner, env, root, assembler, gencmd

source, reference = root/'examples/wash15.asm', root/'examples/ref/was15.a86'
hashes = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in (source, reference)}
report = ['# WASH file-operation comparison', '',
          'Source and reference are unchanged. Each run starts from isolated A/B fixtures.',
          'Complete transcripts and file inventories/bytes are compared independently.', '']
cases = [('rename', b'Rgamma.dat\rX', b'GAMMA'),
         ('rename cancel', b'R\rX', b'New Name'),
         ('rename duplicate', b'Rbeta.dat\rX', b'Name Already Exists'),
         ('delete', b'\x04X', b'Deleted'),
         ('copy decline', b'CBNX', b'Replace'),
         ('copy same drive', b'CAX', b'Cannot Select Same Disk'),
         ('copy invalid drive', b'C?X', b'Destination Drive')]
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    a, b = work/'a', work/'b'
    a.mkdir(); b.mkdir()
    env.update(EMU2_DRIVE_A=str(a), EMU2_DRIVE_B=str(b), EMU2_DRIVE_C=str(work), EMU2_DEFAULT_DRIVE='C:')
    command, build, execute = runner(work, 'cpm86')
    (work/'wasref.a86').write_bytes(reference.read_bytes())
    log = command([assembler, 'wasref.a86'])
    assert b'NUMBER OF ERRORS:   0' in log and b'ERROR NO:' not in log, log
    command([gencmd, 'wasref', '8080'])

    def setup(collision=False, payload=b'A'*256):
        for folder in (a, b):
            for path in folder.iterdir(): path.unlink()
        (a/'alpha.dat').write_bytes(payload)
        (a/'beta.dat').write_bytes(b'B'*128)
        if collision: (b/'alpha.dat').write_bytes(b'O'*128)
        env['EMU2_DEFAULT_DRIVE'] = 'A:'

    def snapshot():
        return tuple({p.name.lower(): p.read_bytes() for p in folder.iterdir()} for folder in (a, b))

    baseline = {}
    for name, keys, token in cases:
        setup(name == 'copy decline')
        expected_a, expected_b = snapshot()
        if name == 'rename': expected_a['gamma.dat'] = expected_a.pop('alpha.dat')
        if name == 'delete': del expected_a['alpha.dat']
        output = execute('wasref', 'alpha.dat', input=keys, timeout=10)
        assert token in output and snapshot() == (expected_a, expected_b), (name, output, snapshot())
        baseline[name] = output, snapshot()
    report += ['## Passing comparisons', '', '| Emitter | Result |', '| --- | --- |']
    for flavor in ('nasm', 'asm86', 'masm3', 'drasm86'):
        env['EMU2_DEFAULT_DRIVE'] = 'C:'
        build('w'+flavor, source.read_bytes(), flavor, dialect='m80')
        for name, keys, token in cases:
            setup(name == 'copy decline')
            output = execute('w'+flavor, 'alpha.dat', input=keys, timeout=10)
            assert (output, snapshot()) == baseline[name], (flavor, name, output, baseline[name][0])
        report.append(f'| {flavor} | all seven transcripts and file snapshots match |')
        print('WASH', flavor, 'seven file-operation/cancellation comparisons passed', flush=True)
    report += ['', 'Cases: '+', '.join(name for name, _, _ in cases)+'.', '',
               '## Copy completion diagnostic', '',
               'These probes use independent expected file contents and a ten-second bound. Reference timeouts are not matches.', '',
               '| Program | Copy | Replace |', '| --- | --- | --- |']
    # Probe the actual transfer separately: the reference retains CP/M-80
    # memory-limit arithmetic. A timeout must never be reported as equality.
    for name in ('wasref', 'wnasm', 'wasm86', 'wmasm3', 'wdrasm86'):
        outcomes = []
        for collision, keys in ((False, b'CBX'), (True, b'CBYX')):
            setup(collision)
            try:
                output = execute(name, 'alpha.dat', input=keys, timeout=10)
                complete = snapshot()[1].get('alpha.dat') == b'A'*256
                outcomes.append('completed with matching bytes' if complete else 'returned without a complete copy')
            except subprocess.TimeoutExpired:
                outcomes.append('timed out; no completion claimed')
            assert snapshot()[0] == {'alpha.dat': b'A'*256, 'beta.dat': b'B'*128}
        if name in ('wasm86', 'wmasm3', 'wdrasm86'):
            assert outcomes == ['completed with matching bytes'] * 2, (name, outcomes)
        report.append(f'| {name} | {outcomes[0]} | {outcomes[1]} |')
    payload = bytes(range(256)) * 256 + bytes(range(128))
    for name in ('wasm86', 'wmasm3', 'wdrasm86'):
        setup(payload=payload)
        execute(name, 'alpha.dat', input=b'CBX', timeout=10)
        assert snapshot() == ({'alpha.dat': payload, 'beta.dat': b'B'*128}, {'alpha.dat': payload}), name
    report += ['', '513-record copies also pass on ASM86/MASM3/DRASM86; reference timeouts are not equivalence passes.']

assert all(hashlib.sha256(p.read_bytes()).hexdigest() == h for p,h in hashes.items())
if len(sys.argv) > 1:
    Path(sys.argv[1]).write_text('\n'.join(report)+'\n')
