"""Default-FCB native-memory boundary, independent of application names."""
from pathlib import Path
import subprocess
import tempfile
from toolrun import runner, exe


def program(access=''):
    return ''' org 100h
''' + access + ''' lxi d,handle
 mvi c,15
 call service
 cpi 255
 jz failed
 lxi d,handle
 mvi c,16
 call service
 cpi 255
 jz failed
 lxi d,good
 jmp print
failed: lxi d,bad
print: mvi c,9
 call service
 jmp 0
service equ 5+0
handle equ 5ch
good: db 'DEFAULT OK$'
bad: db 'DEFAULT FAILED$'
 end
'''


with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    _, build, execute = runner(work, 'msdos')
    (work / 'input.dat').write_bytes(b'unchanged')
    for flavor in ('nasm', 'asm86', 'masm3', 'drasm86'):
        build('fcbdef', program(' lxi h,handle+1\n mov a,m\n'), flavor)
        assert b'DEFAULT OK' in execute('fcbdef', 'input.dat')
        assert b'DEFAULT FAILED' in execute('fcbdef', 'missing.dat')
    assert (work / 'input.dat').read_bytes() == b'unchanged'
    for access in (' lda handle+14\n', ' sta 68h\n', ' lhld handle+11\n',
                   ' lxi h,handle+12\n mov a,m\n',
                   ' lxi h,handle+11\n inx h\n mov a,m\n',
                   ' mvi h,0\n mvi l,68h\n mov a,m\n',
                   ' lxi d,handle+11\n inx d\n ldax d\n',
                   ' lxi h,handle\n lxi b,12\n dad b\n mov a,m\n'):
        (work / 'unsafe.80').write_text(program(access))
        (work / 'keep.asm').write_text('preserve')
        result = subprocess.run([str(exe), '--target=msdos', str(work / 'unsafe.80'),
                                 str(work / 'keep.asm')], capture_output=True)
        assert result.returncode and b'metadata access' in result.stderr, result.stderr
        assert (work / 'keep.asm').read_text() == 'preserve'
    (work / 'unsafe.80').write_text(program().replace('lxi d,handle', 'lxi d,6ch', 1))
    (work / 'keep.asm').write_text('preserve')
    result = subprocess.run([str(exe), '--target=msdos', str(work / 'unsafe.80'),
                             str(work / 'keep.asm')], capture_output=True)
    assert result.returncode and b'mixed with another file object' in result.stderr, result.stderr
    assert (work / 'keep.asm').read_text() == 'preserve'
    # A runtime address remains native memory; it is not a CP/M memory view.
    build('rawmem', program(' in 1\n mov l,a\n mvi h,0\n mov a,m\n'), 'nasm')
    print('default FCB: all emitters, names/missing files, explicit field rejection and native-memory boundary passed')
