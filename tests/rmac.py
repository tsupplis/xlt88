"""RMAC single-module placement, macros, provenance and relocation rejection."""
import subprocess
import hashlib
import tempfile
from pathlib import Path
from toolrun import runner, root, exe

source = ''' name ('RMTEST')
 public start,message,finish
 maclib console
 dseg
message: db 'RMAC OK$'
 cseg
start: say message
 jmp 0
 dseg
words: dw start,message,finish-start
 cseg
finish: nop
 end start
'''
library = '''say macro ?text
 lxi d,?text
 mvi c,9
 call 5
 endm
'''
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    (work / 'console.lib').write_text(library)
    legacy = (root / 'tests/parse.asm').read_bytes()
    legacy_hash = hashlib.sha256(legacy).hexdigest()
    (work / 'parse.asm').write_bytes(legacy)
    for target in ('msdos', 'cpm86'):
        _, build, execute = runner(work, target)
        for flavor in ('nasm', 'asm86', 'masm3', 'drasm86'):
            build('rmac', source, flavor, dialect='rmac')
            assert b'RMAC OK' in execute('rmac')
            absolute = " aseg\n org 100h\n lxi d,msg\n mvi c,9\n call 5\n jmp 0\nmsg: db 'ABS OK$'\n end\n"
            build('abs', absolute, flavor, dialect='rmac')
            assert b'ABS OK' in execute('abs')
            # Unchanged DRI CP/M 3 filename parser, included as source in a
            # small executable harness. This does not exercise REL linking.
            for filename, drive, name, extension, password in (
                ('A:foo.txt', 1, 'FOO     ', 'TXT', ''),
                ('*.asm', 0, '????????', 'ASM', ''),
                ('b:doc.bin;PASS', 2, 'DOC     ', 'BIN', 'PASS'),
                ('Q:BAD.TXT', -1, '', '', ''),
            ):
                harness = f""" dseg
args: dw filename,fcb
filename: db '{filename}',0
fcb: ds 36
guard: db 165
msg: db 'PARSE OK$'
 cseg
start: lxi b,args
 call parse
 mov a,h
 cpi {255 if drive < 0 else 0}
 jnz fail
 mov a,l
 cpi {255 if drive < 0 else 0}
 jnz fail
"""
                if drive >= 0:
                    expected = bytes([drive]) + (name + extension).encode() + bytes(4) + password.ljust(8).encode()
                    for offset, byte in enumerate(expected):
                        harness += f' lda fcb+{offset}\n cpi {byte}\n jnz fail\n'
                    harness += f' lda fcb+26\n cpi {len(password)}\n jnz fail\n'
                harness += " lda guard\n cpi 165\n jnz fail\n lxi d,msg\n mvi c,9\n call 5\nfail: jmp 0\n include parse.asm\n"
                build('legacy', harness, flavor, dialect='rmac')
                assert b'PARSE OK' in execute('legacy'), (target, flavor, filename)
        print(target, 'RMAC CSEG/DSEG, absolute module and MACLIB execution passed on all emitters', flush=True)
    assert hashlib.sha256((work / 'parse.asm').read_bytes()).hexdigest() == legacy_hash
    input_file, output = work / 'input.asm', work / 'out.asm'

    def invoke(text, expected=None):
        input_file.write_text(text)
        output.write_text('preserve')
        command = [str(exe), '--dialect=rmac', '--emit=8080', str(input_file), str(output)]
        result = subprocess.run(command, capture_output=True)
        if expected:
            assert result.returncode and expected.encode() in result.stderr, result.stderr
            assert output.read_text() == 'preserve'
        else:
            assert result.returncode == 0, result.stderr
        return result

    invoke(source)
    normalized = output.read_text()
    assert normalized.index('start:') < normalized.index('finish:') < normalized.index('message:') < normalized.index('words:')
    # A normalized flat source must remain usable by the common Intel reader.
    result = subprocess.run([str(exe), '--target=cpm86', str(output), str(work / 'again.asm')], capture_output=True)
    assert result.returncode == 0, result.stderr
    invoke(' if 0\n extrn missing\n endif\n nop\n end\n')
    invoke('seg macro\n dseg\n endm\n jmp 0\n seg\nx: ds 3\n end\n')
    invoke("$title 'bad\x00title'\n end\n", 'NUL byte')
    invoke("$title '" + 'x' * 260 + "'\n end\n", '254 characters')
    for text, diagnostic in (
        (' extrn outside\n end\n', 'linker support'),
        (' common /work/\n end\n', 'linker support'),
        (' nop\n aseg\n org 200h\n db 0\n end\n', 'mixed absolute'),
        (' cseg\n org 100h\n end\n', 'ORG in a relocatable'),
        (' public missing\n end\n', 'must be defined'),
        (' public abcdefg,abcdefh\nabcdefg: nop\nabcdefh: nop\n end\n', 'first six'),
        ('code: nop\n dseg\ndata: ds 1\nx equ data-code\n end\n', 'relocation expression'),
        ('code: nop\nx equ code*2\n end\n', 'relocation expression'),
        ('code: nop\n db code\n end\n', 'relocation expression'),
        ('here cseg\n end\n', 'section labels'),
        (' maclib missing\n end\n', 'cannot open included'),
    ):
        invoke(text, diagnostic)
    (work / 'broken.lib').write_text(' extrn missing\n')
    result = invoke(' maclib broken\n end\n', 'linker support')
    assert b'broken.lib:1:' in result.stderr
    # Stateful names retain physical provenance even though sections move.
    input_file.write_text(source)
    result = subprocess.run([str(exe), '--dialect=rmac', '--state-out=' + str(work / 'names.sym'),
                             str(input_file), str(output)], capture_output=True)
    assert result.returncode == 0, result.stderr
    result = subprocess.run([str(exe), '--dialect=rmac', '--state-in=' + str(work / 'names.sym'),
                             str(input_file), str(work / 'named.asm')], capture_output=True)
    assert result.returncode == 0 and output.read_bytes() == (work / 'named.asm').read_bytes(), result.stderr
print('RMAC normalization/state round trip, diagnostics and atomic rejection passed')
