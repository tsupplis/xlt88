"""Execute the explicit console-only FIG-Forth port with all toolchains."""
import hashlib
from pathlib import Path
import re
import sys
import tempfile
from toolrun import runner
root = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'ports'))
from figforth import port_source

original = root/'examples'/'forth.asm'
before = hashlib.sha256(original.read_bytes()).digest()
source = port_source(original.read_text())
script = b'123 456 + . : SQUARE DUP * ; 7 SQUARE . BYE\r'
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    for target in ('msdos','cpm86'):
        _, build, execute = runner(work,target)
        for flavor in ('nasm','asm86','masm3','drasm86'):
            build('forth',source,flavor,dialect='m80')
            result = execute('forth',input=script)
            assert b'fig-FORTH 1.1' in result, (target,flavor,result)
            assert re.search(rb'BYE\s+579\s+49\s*$',result), (target,flavor,result)
            # Disk requests must terminate explicitly, never pretend success.
            result = execute('forth',input=b'1 BLOCK BYE\r')
            assert b'Unsupported disk/printer request' in result, (target,flavor,result)
            print(f'{flavor}/{target}: Forth startup, arithmetic, definition, BYE and disk rejection passed',flush=True)
assert hashlib.sha256(original.read_bytes()).digest() == before
