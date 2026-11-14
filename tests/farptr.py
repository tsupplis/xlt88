"""Native far-result ABI with explicit test-only target consumer adjustments.

This tests native entry-point lowering, not automatic 8080 pointer conversion.
A raw INT E0h serves as the ABI oracle; the test inserts segment-aware reads.
"""
from pathlib import Path
import subprocess
import tempfile
from toolrun import runner, exe

source = ''' org 100h
prepare: mvi c,FUNCTION
 call 5
probe: nop
 lxi d,good
 mvi c,9
 call 5
 jmp 0
failed: lxi d,bad
 mvi c,9
 call 5
 jmp 0
good: db 'POINTER OK$'
bad: db 'POINTER FAILED$'
 end
'''
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    _, build, execute = runner(work, 'cpm86')
    for flavor in ('nasm', 'asm86', 'masm3', 'drasm86'):
        for function, dynamic in ((27, False), (31, False), (27, True), (31, True)):
            text = source.replace('FUNCTION', str(function))
            if dynamic:
                text = text.replace(f'prepare: mvi c,{function}', 'prepare: lda selector\n mov c,a')
                text = text.replace(' end\n', f'selector: db {function}\n end\n')
            (work / 'pointer.80').write_text(text)
            result = subprocess.run([str(exe), '--target=cpm86', '--emit='+flavor,
                                     str(work/'pointer.80'), str(work/'check.asm')], capture_output=True)
            warning = b'native BDOS pass-through' if dynamic else b'manual port review required'
            assert result.returncode == 0 and warning in result.stderr, result.stderr

            def patch(assembly):
                # Poison incoming ES: restoring it would destroy the result.
                prefix = '$' if flavor == 'nasm' else 's'
                prepare, probe, failed = (prefix + name for name in ('prepare', 'probe', 'failed'))
                assert assembly.count(prepare + ':') == assembly.count(probe + ':') == 1
                assembly = assembly.replace(prepare + ':', prepare + ':\n\txor ax,ax\n\tmov es,ax\n')
                memory = '[es:bx]' if flavor == 'nasm' else 'es:[bx]'
                check = f'''{probe}:
 mov ax,es
 or ax,ax
 jz {failed}
 mov si,ax
 mov di,bx
 mov dl,{memory}
 push dx
 mov cl,{function}
 int 224
 pop dx
 mov ax,es
 cmp ax,si
 jne {failed}
 cmp bx,di
 jne {failed}
 mov al,{memory}
 cmp al,dl
 jne {failed}
 push bx
 mov cl,11
 int 224
 pop bx
 mov ax,es
 cmp ax,si
 jne {failed}
'''
                return assembly.replace(probe + ':', check)

            build('pointer', text, flavor, patch=patch)
            output = execute('pointer')
            assert b'POINTER OK' in output and b'FAILED' not in output, (flavor, function, output)
        print(flavor, '27/31 known/dynamic native ES:BX, explicit far reads and later ES preservation passed', flush=True)
    # DOS must keep these CP/M system-data interfaces unsupported.
    (work/'pointer.80').write_text(source.replace('FUNCTION', '31'))
    result = subprocess.run([str(exe), '--target=msdos', str(work/'pointer.80'),
                             str(work/'dos.asm')], capture_output=True)
    assert result.returncode and b'needs a native port' in result.stderr
