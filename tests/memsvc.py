"""Memory-loaded service candidates: all valid arms and unmatched termination."""
from pathlib import Path
import subprocess
import tempfile
from toolrun import runner, exe

source = ''' org 100h
 mvi a,CHOICE
 ora a
 jz string
 mvi a,2
 mvi e,'X'
 jmp save
string: mvi a,9
 lxi d,msg
save: sta selector
 call disturb
 MUTATE
 lda selector
 mov c,a
 call 5
 lxi d,done
 mvi c,9
 call 5
 jmp 0
disturb: mvi a,255
 ret
selector:
 ds 1
msg: db 'STRING$'
done: db 'DONE$'
 end
'''
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    for target in ('cpm86', 'msdos'):
        _, build, execute = runner(work, target)
        for flavor in ('nasm', 'asm86', 'masm3', 'drasm86'):
            for choice, expected in ((0, b'STRINGDONE'), (1, b'XDONE')):
                build('memsvc', source.replace('CHOICE', str(choice)).replace('MUTATE', ''), flavor)
                output = execute('memsvc')
                assert expected in output, (target, flavor, choice, output)
            renamed = source.replace('selector', 'routebyte').replace('save:', 'storeit:').replace('jmp save', 'jmp storeit')
            build('renamed', renamed.replace('CHOICE', '0').replace('MUTATE', ''), flavor)
            assert b'STRINGDONE' in execute('renamed')
            # An indirect write is native memory, not a proof of a stored value.
            # It must never fall through to the final supported service arm.
            mutation = 'lxi h,selector\n mvi m,255'
            build('memsvc', source.replace('CHOICE', '0').replace('MUTATE', mutation), flavor)
            output = execute('memsvc')
            assert b'STRING' not in output and b'DONE' not in output, output
            # A singleton candidate still needs a check.
            singleton = source.replace('mvi a,2\n', 'mvi a,9\n')
            build('memsvc', singleton.replace('CHOICE', '1').replace('MUTATE', mutation), flavor)
            assert b'DONE' not in execute('memsvc')
            print(target, flavor, 'stored selectors: both arms, alias mutation and singleton guard passed', flush=True)
    for bad in (source.replace('mvi a,2\n', 'in 1\n'), source.replace('mvi a,2\n', 'mvi a,38\n')):
        (work / 'bad.80').write_text(bad.replace('CHOICE', '1').replace('MUTATE', ''))
        (work / 'keep.asm').write_text('preserve')
        result = subprocess.run([str(exe), '--target=cpm86', str(work / 'bad.80'), str(work / 'keep.asm')], capture_output=True)
        assert result.returncode and (work / 'keep.asm').read_text() == 'preserve', result.stderr
