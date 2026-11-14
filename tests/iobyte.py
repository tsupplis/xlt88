"""Native CP/M-86 IOBYTE: restore routing before any console report."""
import os
from pathlib import Path
import tempfile
from toolrun import runner

source = ''' org 100h
 mvi c,7
 call 5
 sta saved
 xri 40h
 mov e,a
 mvi c,8
 call 5
 mvi c,7
 call 5
 xri 40h
 mov b,a
 lda saved
 cmp b
 jnz failed
 mvi a,1
 sta passed
failed: lda saved
 mov e,a
 mvi c,8
 call 5
 lda passed
 ora a
 jz badmsg
 lxi d,good
 jmp print
badmsg: lxi d,bad
print: mvi c,9
 call 5
 jmp 0
saved: db 0
passed: db 0
good: db 'IOBYTE OK$'
bad: db 'IOBYTE FAILED$'
 end
'''
with tempfile.TemporaryDirectory() as directory:
    _, build, execute = runner(Path(directory), 'cpm86')
    for flavor in ('nasm', 'asm86', 'masm3', 'drasm86'):
        build('iobyte', source, flavor)
        if os.environ.get('CPM_IOBYTE') == '1':
            output = execute('iobyte')
            assert b'IOBYTE OK' in output and b'FAILED' not in output, output
        print(flavor, 'IOBYTE get/set assembled; ' + ('execution passed' if os.environ.get('CPM_IOBYTE') == '1' else 'execution unverified (CPM_IOBYTE=1 requires native routing state)'), flush=True)
