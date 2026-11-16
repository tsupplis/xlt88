"""Execute differing native sequences and fresh source-contract probes.

Native pairs retain the inspected instructions (names/syntax normalized to
NASM). Source probes are separately labelled: observation can change XLT86's
liveness choices, so they do not prove the original application context.
"""
import os
from pathlib import Path
import re
import sys
import tempfile
from arith import fixture, reference
from toolrun import runner, assembler, gencmd, env
from xltref import legacy_text


def translated_pair(work, source):
    command, build, execute = runner(work, 'cpm86')
    (work/'probe.asm').write_bytes(legacy_text(source))
    log = command([os.environ.get('XLT86', 'cpm_xlt86'), 'probe'])
    assert b'Translate-86' in log and b'Number of Errors:' not in log, log
    log = command([assembler, 'probe'])
    assert b'NUMBER OF ERRORS:   0' in log and b'ERROR NO:' not in log, log
    command([gencmd, 'probe', '8080'])
    old = execute('probe').strip()
    build('new', source, 'asm86')
    return old, execute('new').strip()


def source_probes(work, report):
    probes = [('sub', 'immediate', (16, 1, 2)),
              ('cmp', 'immediate', (16, 1, 2)),
              ('sbb', 'b', (16, 1, 3)),
              ('dcr', 'a', (16, 0, 2)),
              ('ana', 'a', (8, 8, 2)),
              ('ana', 'immediate', (0, 127, 2))]
    report += ['| Fresh source probe | Expected A/PSW | XLT86 | xlt88 |',
               '| --- | --- | --- | --- |']
    for op, form, values in probes:
        source = fixture(op, form, [values])
        old, new = translated_pair(work, source)
        actual = old.decode()
        expected = reference(op, *values)
        if actual == 'OK':
            observed = expected
        else:
            assert re.fullmatch(r'0000[0-9A-F]{4}', actual), actual
            observed = (int(actual[4:6], 16), int(actual[6:8], 16))
        assert new == b'OK', (op, form, new)
        fmt = lambda pair: f'{pair[0]:02X}/{pair[1]:02X}'
        report.append(f'| {op} {form}, {values} | {fmt(expected)} | {fmt(observed)} | {fmt(expected)} |')
    report += ['', 'Expected values use the existing 8080 reference model. These are six witnesses, not exhaustive CPU coverage.', '']


def contract_probes(work, report):
    for drive in ('A', 'B', 'C'):
        env['EMU2_DRIVE_'+drive] = str(work)
    suffix = """ lxi d,good
 jmp print
failed: lxi d,bad
print: mvi c,9
 call 5
 jmp 0
good: db 'OK$'
bad: db 'FAIL$'
 end
"""
    probes = [
        ('PUSH PSW with A immediately live', """ lxi h,55d7h
 push h
 pop psw
 push psw
 mov b,a
 pop psw
 cpi 55h
 jnz failed
 mov a,b
 cpi 55h
 jnz failed
"""),
        ('BDOS 13 selects drive A', """ mvi e,1
 mvi c,14
 call 5
 mvi c,13
 call 5
 mvi c,25
 call 5
 ora a
 jnz failed
"""),
        ('BDOS 6 outputs FEh', """ mvi e,254
 mvi c,6
 call 5
"""),
    ]
    report += ['| Fresh context/OS probe | XLT86 | xlt88 |', '| --- | --- | --- |']
    for name, body in probes:
        old, new = translated_pair(work, ' org 100h\n'+body+suffix)
        if 'FEh' in name:
            assert b'\xfe' in new, new
            a, b = ('FEh output' if b'\xfe' in output else 'no FEh output' for output in (old, new))
        else:
            assert new == b'OK', (name, new)
            a, b = old.decode(), new.decode()
        report.append(f'| {name} | {a} | {b} |')
    report += ['', 'OS results are observations under the configured CP/M-86 emulator, not hardware validation.', '']


def native(work, code, checks, setup='', flags=True):
    """Observe source-visible registers, memory and S/Z/AC/P/C immediately."""
    command, _, execute = runner(work, 'msdos')
    lines = ['bits 16', 'cpu 8086', 'org 100h']
    # All 32 combinations of the five source flags; both carry outcomes for
    # doubling, and positive/negative/wrap boundaries for word increments.
    for index, (status, word) in enumerate((s | 2, w)
            for s in range(256) if not s & ~0xd5
            for w in (0, 0x7fff, 0x8000, 0xffff)):
        lines += [f'case{index}:', 'mov cx,5', 'mov dx,8000h', f'mov bx,{word}',
                  setup.format(word=word), f'mov ax,{status * 256 + 0x55}', 'sahf', code,
                  'pushf', 'pop di', checks.format(word=word, inc=(word+3)&65535,
                                                twice=(word*2+(5 if word & 32768 else 0))&65535,
                                                double=(word*2)&65535, zpath=2 if status & 64 else 1)]
        if flags:
            lines += ['and di,0d5h', f'cmp di,{status & 0xd5}', 'jne failed']
    lines += ['mov dx,good', 'jmp print', 'failed: mov dx,bad',
              'print: push cs', 'pop ds', 'mov ah,9', 'int 21h', 'int 20h',
              "good: db 'OK$'", "bad: db 'FAIL$'", 'align 2',
              'guard: db 0a5h', 'cell: db 11h,22h,33h', 'after: db 5ah',
              'times 8192 db 0']
    (work/'native.asm').write_text('\n'.join(lines))
    command([os.environ.get('NASM', 'nasm'), '-f', 'bin', 'native.asm', '-o', 'native.com'])
    result = execute('native').strip().decode()
    assert result in ('OK', 'FAIL'), result
    return result


def native_probes(work, report):
    reg_check = 'cmp al,55h\njne failed\ncmp cx,5\njne failed\n'
    pairs = [
        ('Short conditional versus inverted condition plus JMP',
         'jz .taken\nmov bx,1\njmp .done\n.taken: mov bx,2\n.done:',
         'jnz .skip\njmp .taken\n.skip: mov bx,1\njmp .done\n.taken: mov bx,2\n.done:',
         reg_check+'cmp bx,{zpath}\njne failed', ''),
        ('Three word increments',
         '\n'.join(['lahf\ninc bx\nsahf']*3),
         'lahf\ninc bx\ninc bx\ninc bx\nsahf',
         reg_check+'cmp bx,{inc}\njne failed\ncmp dx,8000h\njne failed', ''),
        ('PSW save/restore around doubling and carry branch',
         'lahf\nxchg al,ah\npush ax\nshl bx,1\nxchg bx,dx\nshl bx,1\nxchg bx,dx\njnc .skip\nadd bx,cx\n.skip:\npop ax\nxchg al,ah\nsahf',
         'lahf\nxchg al,ah\npush ax\nxchg al,ah\nadd bx,bx\nxchg bx,dx\nadd bx,bx\nxchg bx,dx\njnc .skip\nadd bx,cx\n.skip:\npop ax\nxchg al,ah\nsahf',
         reg_check+'cmp bx,{twice}\njne failed\ncmp dx,{double}\njne failed', 'mov dx,{word}'),
        ('Byte stores through SI versus DI, odd address and neighbours',
         'mov si,bx\nmov [si],al\nmov al,[si]',
         'mov di,bx\nmov [di],al\nmov al,[di]',
         reg_check+'cmp byte [guard],0a5h\njne failed\ncmp byte [cell+1],22h\njne failed\ncmp byte [cell+2],33h\njne failed\ncmp byte [after],5ah\njne failed',
         'mov bx,cell'),
        ('CS versus DS direct access with shared segments',
         'mov al,[cs:cell+1]', 'mov al,[cell+1]',
         'cmp al,22h\njne failed\ncmp cx,5\njne failed', ''),
        ('Odd-address word read/write with byte canaries, shared segments',
         'mov [cs:cell],bx\nmov bx,[cs:cell]',
         'mov [cell],bx\nmov bx,[cell]',
         reg_check+'cmp bx,{word}\njne failed\ncmp byte [guard],0a5h\njne failed\ncmp byte [cell+2],33h\njne failed\ncmp byte [after],5ah\njne failed', ''),
        ('PSW push: A observed before restoration (deliberately stricter context)',
         'lahf\nxchg al,ah\npush ax\npop si',
         'lahf\nxchg al,ah\npush ax\nxchg al,ah\npop si',
         reg_check, ''),
    ]
    report += ['| Selected native sequence | XLT86 sequence | xlt88 sequence |',
               '| --- | --- | --- |']
    for name, old, new, checks, setup in pairs:
        results = [native(work, code, checks, setup) for code in (old, new)]
        assert results[1] == 'OK', (name, results)
        if 'stricter' not in name:
            assert results[0] == 'OK', (name, results)
        else:
            assert results[0] == 'FAIL', (name, results)
        report.append(f'| {name} | {results[0]} | {results[1]} |')
    different_ds = ('push cs\npop ax\nadd ax,100h\nmov ds,ax\n'
                    'mov byte [cs:cell+1],22h\nmov byte [cell+1],77h')
    for code, expected in [('mov al,[cs:cell+1]', 0x22), ('mov al,[cell+1]', 0x77)]:
        assert native(work, code, f'cmp al,{expected}\njne failed', different_ds) == 'OK'
    report.append('| CS versus DS with different segments | reads 22h | reads 77h |')
    for restore in ('', 'sahf\n'):
        # VTL term restores A, then overwrites flags before observing them.
        code = ('lahf\nxchg al,ah\npush ax\nxchg al,ah\nstc\n'
                'pop ax\nxchg al,ah\n'+restore+'xor al,al')
        assert native(work, code, 'cmp al,0\njne failed\nand di,0d5h\ncmp di,44h\njne failed', flags=False) == 'OK'
    report.append('| POP PSW restore omitted before a full flag overwrite | OK | OK |')
    report += ['', 'Passing native variants each complete 128 states (32 PSWs × four word boundaries); negative witnesses stop at the first mismatch.',
               'A FAIL under the deliberately stricter A-observation test is a context requirement, not proof of an application defect.', '']


def main():
    report = ['# XLT86/xlt88 sequence validation', '',
              'Native excerpts and fresh source probes are deliberately distinguished.', '']
    with tempfile.TemporaryDirectory() as directory:
        work = Path(directory)
        source_probes(work, report)
        native_probes(work, report)
        contract_probes(work, report)
    result = '\n'.join(report)+'\n'
    if len(sys.argv) > 1:
        Path(sys.argv[1]).write_text(result)
    print(result)


if __name__ == '__main__':
    main()
