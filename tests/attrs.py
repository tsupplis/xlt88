"""CP/M-86 file-attribute lowering; verify read-only set/clear and contents."""
from pathlib import Path
import tempfile
from toolrun import runner
source = ''' org 100h
 lxi d,fcb
 mvi c,30
 call 5
 cpi 255
 jz failed
 lxi d,good
 mvi c,9
 call 5
 jmp 0
failed: lxi d,bad
 mvi c,9
 call 5
 jmp 0
fcb: db 0,'ATTR    ',EXT,'AT'
 ds 24
good: db 'ATTR OK$'
bad: db 'ATTR FAILED$'
 end
'''
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    _, build, execute = runner(work, 'cpm86')
    file = work/'attr.dat'
    for flavor in ('nasm','asm86','masm3','drasm86'):
        file.write_bytes(b'unchanged data')
        for ext, readonly in ((196,True),(68,False)):
            build('attrs',source.replace('EXT',str(ext)),flavor)
            output = execute('attrs')
            assert b'ATTR OK' in output and b'FAILED' not in output, output
            assert bool(file.stat().st_mode & 0o222) != readonly
            assert file.read_bytes() == b'unchanged data'
        file.unlink()
        output = execute('attrs')
        assert b'ATTR FAILED' in output, output
        print(flavor,'read-only set/clear, unchanged contents and missing-file status passed',flush=True)
