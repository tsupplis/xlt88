"""CP/M-86 pass-through, native contract boundaries and DOS rejection."""
from pathlib import Path
import subprocess
import tempfile
from toolrun import runner, exe, env

with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    for drive in ('A', 'B', 'C'):
        env['EMU2_DRIVE_' + drive] = str(work)
    env['EMU2_DEFAULT_DRIVE'] = 'C:'
    _, build, execute = runner(work, 'cpm86')
    for flavor in ('nasm', 'asm86', 'masm3', 'drasm86'):
        for transfer in ('call 5', 'call helper'):
            source = f''' org 100h
 lda selector
 mov c,a
 lxi d,msg
 {transfer}
 lxi d,done
 mvi c,9
 call 5
 jmp 0
helper: jmp 5
selector: db 9
msg: db 'NATIVE$'
done: db 'DONE$'
 end
'''
            build('native', source, flavor)
            assert b'NATIVEDONE' in execute('native')
        for dynamic in (False, True):
            select = 'lda selector\n mov c,a' if dynamic else 'mvi c,6'
            source = f''' org 100h
 {select}
 mvi e,254
 call 5
 jmp 0
selector: db 6
 end
'''
            build('native', source, flavor)
            output = execute('native')
            assert (b'\xfe' in output) == (not dynamic), (flavor, dynamic, output)
            select = 'lda selector\n mov c,a' if dynamic else 'mvi c,13'
            expected = 1 if dynamic else 0
            source = f''' org 100h
 mvi e,1
 mvi c,14
 call 5
 {select}
 call 5
 mvi c,25
 call 5
 cpi {expected}
 jnz failed
 lxi d,good
 jmp print
failed: lxi d,bad
print: mvi c,9
 call 5
 jmp 0
selector: db 13
good: db 'RESET CONTRACT OK$'
bad: db 'RESET CONTRACT FAILED$'
 end
'''
            build('native', source, flavor)
            assert b'RESET CONTRACT OK' in execute('native')
        # Exercise extension continuation without assuming whether the runner
        # implements function 46. Native service correctness is a separate test.
        source = ''' org 100h
 mvi e,0
 mvi c,46
 call 5
 lxi d,good
 mvi c,9
 call 5
 jmp 0
good: db 'EXTENSION OK$'
bad: db 'EXTENSION FAILED$'
 end
'''
        build('native', source, flavor)
        assert b'EXTENSION OK' in execute('native')
        print(flavor, 'unknown calls/tail calls, native 6/13 contracts and extension continuation passed', flush=True)
    for text in ('lda selector\n mov c,a\n call 5', 'mvi c,46\n call 5'):
        (work/'check.80').write_text(' org 100h\n '+text+'\n jmp 0\nselector: db 9\n end\n')
        result = subprocess.run([str(exe), '--target=cpm86', str(work/'check.80'), str(work/'check.asm')], capture_output=True)
        assert result.returncode == 0 and b'native BDOS pass-through' in result.stderr
        assert 'port review required' in (work/'check.asm').read_text()
        (work/'keep.asm').write_text('preserve')
        result = subprocess.run([str(exe), '--target=msdos', str(work/'check.80'), str(work/'keep.asm')], capture_output=True)
        assert result.returncode and (work/'keep.asm').read_text() == 'preserve'
