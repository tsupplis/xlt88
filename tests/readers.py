#!/usr/bin/env python3
import os
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parent.parent
exe = Path(os.environ.get('XLT88', root / 'xlt88'))
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    intel = "        org 100h\n        lxi d,msg\n        mvi c,9\n        call 5\n        jmp 0\nmsg:    db 'M80 OK$'\n        end\n"
    m80 = ".8080\nTITLE Someone's program\nSUBTTL test\nPAGE\n" + '\n'.join(line.lstrip() if not line.startswith('msg:') else '  ' + line for line in intel.splitlines()) + '\n'
    for target in ('msdos', 'cpm86'):
        binaries = []
        for dialect, content in (('intel', intel), ('m80', m80)):
            src, out, binary = (work / f'{dialect}.{ext}' for ext in ('asm','out','bin'))
            src.write_bytes(content.replace('\n', '\r\n').encode() + b'\x1aPADDING')
            subprocess.run([str(exe), '--dialect='+dialect, '--target='+target, str(src), str(out)], check=True)
            subprocess.run(['nasm', '-Werror', '-f', 'bin', str(out), '-o', str(binary)], check=True)
            binaries.append(binary.read_bytes())
        assert binaries[0] == binaries[1]
    src = work / 'm80.asm'
    out = work / 'norm.asm'
    subprocess.run([str(exe), '--dialect=m80', '--emit=8080', str(src), str(out)], check=True)
    assert ";TITLE Someone's program" in out.read_text()
    subprocess.run([str(exe), str(out), str(work / 'again.asm')], check=True)
print('M80/Intel binary equivalence on both targets and normalized round trip passed')
