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
    for target in ('cpm86', 'msdos'):
        for flavor in ('8080', 'asm86', 'masm3', 'drasm86', 'nasm'):
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
            if flavor == '8080':
                # A normalized file must remain valid translator input.
                subprocess.run([str(exe), '--target=' + target, str(output),
                                str(work / 'round.asm')], check=True,
                               capture_output=True)
print('Assembly text: all non-NASM emitters use CRLF and one final Ctrl-Z; NASM uses LF')
