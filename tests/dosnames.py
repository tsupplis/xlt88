"""Execute native DOS delete/rename from CP/M name records on four emitters."""
import os
from pathlib import Path
import tempfile
from toolrun import runner


def source(function, old, new=None):
    record = " db 0,'" + old + "'\n"
    record += " ds 4\n db 0,'" + new + "'\n ds 8\n" if new else ' ds 24\n'
    return f''' org 100h
 lxi d,record
 mvi c,{function}
 call 5
 cmp l
 jnz bad
 cpi 255
 jz failure
 ora a
 jnz bad
 lxi d,success
 jmp report
failure: lxi d,missing
report: mvi c,9
 call 5
 lda before
 cpi 85
 jnz bad
 lda after
 cpi 170
 jnz bad
 jmp 0
bad: lxi d,error
 mvi c,9
 call 5
 jmp 0
success: db 'SUCCESS$'
missing: db 'FAILURE$'
error: db 'BAD ABI$'
before: db 85
record:
{record}after: db 170
 end
'''

with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    _, build, execute = runner(work,'msdos')
    for flavor in ('nasm','asm86','masm3','drasm86'):
        def run(expected):
            result = execute('names')
            assert result == expected, (flavor,result)
        build('names',source(19,'ONE     DAT'),flavor)
        (work/'one.dat').write_bytes(b'one')
        (work/'keep.dat').write_bytes(b'keep')
        run(b'SUCCESS')
        assert not (work/'one.dat').exists()
        assert (work/'keep.dat').read_bytes() == b'keep'
        run(b'FAILURE')
        build('names',source(19,'DEL?????DAT'),flavor)
        for name in ('del1.dat','del2.dat'):
            (work/name).write_bytes(b'delete')
        run(b'SUCCESS')
        assert not (work/'del1.dat').exists() and not (work/'del2.dat').exists()
        assert (work/'keep.dat').read_bytes() == b'keep'
        build('names',source(23,'OLD     DAT','NEW     DAT'),flavor)
        (work/'old.dat').write_bytes(b'original payload')
        (work/'new.dat').unlink(missing_ok=True)
        run(b'SUCCESS')
        assert not (work/'old.dat').exists()
        assert (work/'new.dat').read_bytes() == b'original payload'
        run(b'FAILURE')
        # Native DOS rejects collisions; installed emu2 uses host rename(),
        # which replaces the destination. Enable this with a corrected emulator.
        if os.environ.get('DOS_RENAME') == '1':
            (work/'old.dat').write_bytes(b'do not overwrite')
            run(b'FAILURE')
            assert (work/'old.dat').read_bytes() == b'do not overwrite'
            assert (work/'new.dat').read_bytes() == b'original payload'
            (work/'old.dat').unlink()
        (work/'new.dat').unlink()
        print(flavor+': DOS delete/wildcard/missing/rename, A=L and guards passed',flush=True)
print('Rename collision: execution unverified unless DOS_RENAME=1; native DOS rejects destination collisions')
