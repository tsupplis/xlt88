#!/usr/bin/env python3
"""Check physical assembly records without universal-newline conversion."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
exe = Path(os.environ.get('XLT88', root / 'xlt88'))
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    help_result = subprocess.run([str(exe), '--help'], capture_output=True)
    assert help_result.returncode == 0 and b'Exit codes:' in help_result.stdout
    assert not help_result.stderr
    assert b'--emit=nasm|asm86|masm3|drasm86|asm80' in help_result.stdout
    assert b'--dialect=asm80|' in help_result.stdout
    for obsolete in ('--emit=8080', '--dialect=intel'):
        result = subprocess.run([str(exe), obsolete], capture_output=True)
        assert result.returncode == 1 and b'unknown option' in result.stderr
    bad = subprocess.run([str(exe), '--unknown'], capture_output=True)
    assert bad.returncode == 1 and b'unknown option: --unknown' in bad.stderr
    # Extension selection is case-insensitive and independent of OS choice.
    # Explicit selection always wins, including normalized 8080 output.
    for suffix, options, expected in (
        ('a86', [], 'asm86'), ('A86', [], 'asm86'),
        ('asm', [], 'nasm'), ('out', [], 'nasm'),
        ('a86', ['--emit=nasm'], 'nasm'),
        ('asm', ['--emit=asm86'], 'asm86'),
        ('a86', ['--emit=asm80'], 'asm80'),
    ):
        for target in ('msdos', 'cpm86'):
            output, baseline = work/('auto.'+suffix), work/'explicit.out'
            common = [str(exe), '--target='+target]
            source = str(root/'examples/hello.asm')
            subprocess.run([*common, *options, source, str(output)], check=True, capture_output=True)
            subprocess.run([*common, '--emit='+expected, source, str(baseline)], check=True, capture_output=True)
            assert output.read_bytes() == baseline.read_bytes(), (suffix, options, target)
    for target in ('cpm86', 'msdos'):
        for flavor in ('asm80', 'asm86', 'masm3', 'drasm86', 'nasm'):
            output = work / 'out.asm'
            subprocess.run([str(exe), '--target=' + target,
                            '--emit=' + flavor, str(root / 'examples/hello.asm'),
                            str(output)], check=True, capture_output=True)
            raw = output.read_bytes()
            if flavor == 'nasm':
                assert raw.endswith(b'\n') and b'\r' not in raw
                assert b'\x1a' not in raw
            else:
                assert raw.endswith(b'\r\n\x1a'), (target, flavor)
                assert raw.count(b'\x1a') == 1, (target, flavor)
                records = raw[:-1].replace(b'\r\n', b'')
                assert b'\r' not in records and b'\n' not in records
            if flavor == 'asm80':
                # A normalized file must remain valid translator input.
                subprocess.run([str(exe), '--target=' + target, str(output),
                                str(work / 'round.asm')], check=True,
                               capture_output=True)
print('Assembly text: all non-NASM emitters use CRLF and one final Ctrl-Z; NASM uses LF')
