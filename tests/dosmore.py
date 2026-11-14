"""DOS random write and file size, with source CP/M record fields."""
import os
from pathlib import Path
import subprocess
import tempfile
from toolrun import runner, exe


def call(function, status=None):
    text = f' lxi d,file\n mvi c,{function}\n call 5\n'
    if status is not None:
        text += f' cmp l\n jnz failed\n cpi {status}\n jnz failed\n'
    return text


def fixture(body, default):
    setup = '' if default else ' lxi h,5ch\n lxi d,file\n call name\n'
    storage = 'file equ 5ch\n' if default else 'file: ds 36\n'
    return ' org 100h\n' + setup + body + ''' lxi d,good
 jmp print
failed: lxi d,bad
print: mvi c,9
 call 5
 jmp 0
name: mvi b,12
next: mov a,m
 stax d
 inx h
 inx d
 dcr b
 jnz next
 ret
good: db 'FILE OK$'
bad: db 'FILE FAILED$'
''' + storage + 'buffer: ds 128\n end\n'


with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    _, build, execute = runner(work, 'msdos')
    for flavor in ('nasm', 'asm86', 'masm3', 'drasm86'):
        for default in (False, True):
            for length in (None, 0, 1, 127, 128, 129, 129 * 128, 65536 * 128 + 1):
                path = work / 'size.dat'
                path.unlink(missing_ok=True)
                if length is not None:
                    with path.open('wb') as stream:
                        stream.truncate(length)
                count = 0 if length is None else (length + 127) // 128
                body = call(35)
                for offset in range(3):
                    body += f' lda file+{33 + offset}\n cpi {(count >> (8 * offset)) & 255}\n jnz failed\n'
                body += f' lhld file+33\n mov a,l\n cpi {count & 255}\n jnz failed\n'
                build('size', fixture(body, default), flavor)
                output = execute('size', 'size.dat')
                assert b'FILE OK' in output, (flavor, default, length, output)
                assert (path.stat().st_size == length) if length is not None else not path.exists()
            if default:
                # A failed size lookup must clear a previously nonzero count.
                path = work / 'vanish.dat'
                path.write_bytes(b'one record')
                body = call(35) + ' lda file+33\n cpi 1\n jnz failed\n'
                body += call(19, 0) + call(35)
                for offset in range(33, 36):
                    body += f' lda file+{offset}\n ora a\n jnz failed\n'
                build('vanish', fixture(body, default), flavor)
                assert b'FILE OK' in execute('vanish', 'vanish.dat')
                assert not path.exists()
            for position in (0, 129):
                data = b''.join(bytes([i % 251]) * 128 for i in range(position + 3))
                path = work / 'write.dat'
                path.write_bytes(data)
                body = call(15, 0) + ' lxi d,buffer\n mvi c,26\n call 5\n'
                body += call(20, 0) * position + call(36)
                body += call(20, 0) * 2 + call(34, 0) * 2
                if os.environ.get('DOS_RSEQ') == '1':
                    body += call(20, 0)
                    body += f' lda buffer\n cpi {(position + 1) % 251}\n jnz failed\n'
                body += call(16, 0)
                build('random', fixture(body, default), flavor)
                output = execute('random', 'write.dat')
                assert b'FILE OK' in output, (flavor, default, position, output)
                start = position * 128
                expected = data[:start] + data[start + 128:start + 256] + data[start + 128:]
                assert path.read_bytes() == expected, (flavor, default, position)
        print(flavor, 'private/default FCB: random overwrite/bookmark/extent and size/missing/24-bit count passed', flush=True)
    # Reading across the random-field boundary or writing incompatible fields
    # must remain an atomic rejection on either storage path.
    for default in (False, True):
        for access in (' lhld file+35\n', ' lda file+32\n', ' sta file+14\n'):
            text = fixture(call(35) + access, default)
            (work / 'bad.80').write_text(text)
            (work / 'keep.asm').write_text('preserve')
            result = subprocess.run([str(exe), '--target=msdos', str(work / 'bad.80'),
                                     str(work / 'keep.asm')], capture_output=True)
            assert result.returncode and b'FCB' in result.stderr, result.stderr
            assert (work / 'keep.asm').read_text() == 'preserve'
