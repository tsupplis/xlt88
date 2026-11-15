#!/usr/bin/env python3
import os
from pathlib import Path
import subprocess
import tempfile
from toolrun import hex_image
root = Path(__file__).resolve().parent.parent
exe = Path(os.environ.get('XLT88', root / 'xlt88'))
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    asm80 = "        org 100h\n        lxi d,msg\n        mvi c,9\n        call 5\n        jmp 0\nmsg:    db 'M80 OK$'\n        end\n"
    m80 = ".8080\nTITLE Someone's program\nSUBTTL test\nPAGE\n" + '\n'.join(line.lstrip() if not line.startswith('msg:') else '  ' + line for line in asm80.splitlines()) + '\n'
    for target in ('msdos', 'cpm86'):
        binaries = []
        for dialect, content in (('asm80', asm80), ('m80', m80)):
            src, out, binary = (work / f'{dialect}.{ext}' for ext in ('asm','out','bin'))
            src.write_bytes(content.replace('\n', '\r\n').encode() + b'\x1aPADDING')
            subprocess.run([str(exe), '--dialect='+dialect, '--target='+target, str(src), str(out)], check=True)
            subprocess.run(['nasm', '-Werror', '-f', 'bin', str(out), '-o', str(binary)], check=True)
            binaries.append(binary.read_bytes())
        assert binaries[0] == binaries[1]
    src = work / 'm80.asm'
    out = work / 'norm.asm'
    subprocess.run([str(exe), '--dialect=m80', '--emit=asm80', str(src), str(out)], check=True)
    assert ";TITLE Someone's program" in out.read_text()
    subprocess.run([str(exe), str(out), str(work / 'again.asm')], check=True)
    # Normalized expressions must use real CP/M ASM operators, not just
    # spellings accepted by our own reader. Include punctuation in a string.
    src.write_text(" org 100h\n db 1 SHL 3,8 SHR 1,5 MOD 2,1 OR 2,3 XOR 1,(NOT 0) AND 255,'>>|^%'\n end\n")
    subprocess.run([str(exe), '--emit=asm80', str(src), str(out)], check=True)
    text = out.read_text()
    assert all(op in text for op in ('SHL', 'SHR', 'MOD', 'OR', 'XOR'))
    result = subprocess.run([os.environ.get('ASM80', 'cpm_asm80'), 'norm'],
                            cwd=work, capture_output=True, timeout=45, check=True)
    assert hex_image(work/'norm.hex') == bytes((8, 4, 1, 3, 2, 255))+b'>>|^%', result.stdout
print('M80/ASM80 binary equivalence on both targets and normalized round trip passed')
