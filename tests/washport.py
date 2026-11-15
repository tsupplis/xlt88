"""Explicit WASH native allocation port; never an automatic translation claim."""
import hashlib
from pathlib import Path
import re
import sys
import tempfile
from toolrun import runner, root, env


def allocation_port(text, flavor):
    """Apply the reviewed target adjustments to an isolated generated file.

    The maintained WASH port motivates these changes. No production translator
    recognizes these labels. Keep its 12-byte directory entries and file logic.
    Use the native inclusive limit as a conservative exclusive limit: leaving
    one byte unused avoids representing 10000h in a 16-bit pointer.
    """
    nasm = flavor == 'nasm'
    label = lambda name: '$' + name.upper() if nasm else 's' + name.lower()
    byte = 'byte' if nasm else 'byte ptr'
    start = label('restart') + ':'
    helper = f'''\tsti
; Test fixture: emulate a smaller native allocation descriptor. emu2 grants
; every 8080-model program at least 64 KiB regardless of CMD maximum size.
\tmov ax,0beefh
\txor di,di
\tmov [di],ax
\tinc di
\tinc di
\tmov {byte} [di],0
\tmov di,0cafeh
\tmov cx,0babeh
\tjcxz wp_init_done
wp_init_guard:
\tmov {byte} [di],0a5h
\tinc di
\tloop wp_init_guard
wp_init_done:
\tjmp {label('restart')}
wp_top:
\tpush di
\tmov di,2
\tmov ax,0ffffh
\tcmp {byte} [di],0
\tjne wp_ready
\txor di,di
\tmov ax,[di]
wp_ready:
\tpop di
\tret
'''
    assert text.count(start) == 1
    text = text.replace(start, helper + start)
    # Preserve emitter-generated memory operands and any location anchors.
    begin = text.index(label('copy4') + ':')
    end = text.index(';CHECK IF ANY MEMORY FOR COPY AT ALL', begin)
    end = text.rfind('\n', begin, end) + 1
    block = text[begin:end]
    operand = re.search(r'mov bx,\s*(.*?)\s*;START ADDRESS OF BUFFER', block)[1]
    anchors = ''.join(line + '\n' for line in block.splitlines() if ' equ ' in line)
    replacement = f'''{label('copy4')}:
{anchors}\tcall wp_top
\tmov bx,{operand}
\tsub ax,bx
\tjnc wp_count
\txor ax,ax
wp_count:
\tmov cl,7
\tshr ax,cl
\tmov cx,ax
'''
    text = text[:begin] + replacement + text[end:]
    operand = re.search(r'mov bx,\s*(.*?)\s*;POINTER TO CURRENT LOAD POINT IN FNLIST', text)[1]
    guard = f'''
\tpush ax
\tpush dx
\tcall wp_top
\tmov dx,{operand}
\tadd dx,140
\tjc wp_full
\tcmp dx,ax
\tjbe wp_fits
wp_full:
\tpop dx
\tpop ax
\tcall {label('prtmsg')}
\tdb 13,10,'Directory too large for memory',13,10,0
\tjmp {label('exit')}
wp_fits:
\tpop dx
\tpop ax
'''
    marker = label('nm2lst') + ':'
    assert text.count(marker) == 1
    text = text.replace(marker, marker + guard)
    # Insertion lengthens this original short back edge beyond 128 bytes.
    branch = '\tjnz ' + label('nm2lst')
    assert text.count(branch) == 1
    text = text.replace(branch, '\tjz wp_search_done\n\tjmp ' + label('nm2lst') + '\nwp_search_done:')
    check = f'''
\tmov di,0cafeh
\tmov cx,0babeh
\tjcxz wp_guard_done
wp_check_guard:
\tcmp {byte} [di],0a5h
\tjne wp_guard_bad
\tinc di
\tloop wp_check_guard
\tjmp wp_guard_done
wp_guard_bad:
\tcall {label('prtmsg')}
\tdb 'GUARD FAILED',0
wp_guard_done:
'''
    marker = label('exit') + ':'
    assert text.count(marker) == 1
    return text.replace(marker, marker + check)


source = root / 'examples/wash15.asm'
digest = hashlib.sha256(source.read_bytes()).hexdigest()
report = ['# WASH explicit native allocation port', '',
          'This is acceptance of a test-only target port, not unchanged-source translation.',
          'The original source/reference and production translator are unchanged by these adjustments.',
          'Reduced allocations are simulated by overriding the base-page descriptor in disposable test binaries: emu2 grants at least 64 KiB. Sixteen-byte canaries check writes beyond the simulated limit.', '',
          '| Emitter | Copy and replace | Directory bounds |', '| --- | --- | --- |']
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    a, b = work / 'a', work / 'b'
    a.mkdir(); b.mkdir()
    env.update(EMU2_DRIVE_A=str(a), EMU2_DRIVE_B=str(b), EMU2_DRIVE_C=str(work))
    command, build, execute = runner(work, 'cpm86')

    def setup(payload, collision=False):
        for folder in (a, b):
            for path in folder.iterdir():
                path.unlink()
        (a / 'alpha.dat').write_bytes(payload)
        if collision:
            (b / 'alpha.dat').write_bytes(b'OLD'.ljust(128, b'!'))
        env['EMU2_DEFAULT_DRIVE'] = 'A:'

    def inventory(folder):
        return {p.name.lower(): p.read_bytes() for p in folder.iterdir()}

    for flavor in ('nasm', 'asm86', 'masm3', 'drasm86'):
        env['EMU2_DEFAULT_DRIVE'] = 'C:'
        build('wport', source.read_bytes(), flavor, dialect='m80',
              patch=lambda text: allocation_port(text, flavor))
        cmd = work / 'wport.cmd'
        image = cmd.read_bytes()
        assert image[0] == 1 and image[9] == 0
        loaded = int.from_bytes(image[1:3], 'little') * 16

        def allocate(size):
            assert loaded <= size <= 65536 and size % 16 == 0
            data = bytearray(image)
            # Patch only distinctive fixture immediates, never application code.
            for marker, value, occurrences in ((b'\xb8\xef\xbe', size - 1, 1),
                                               (b'\xbf\xfe\xca', size & 65535, 2),
                                               (b'\xb9\xbe\xba', 16 if size < 65536 else 0, 2)):
                assert data.count(marker) == occurrences, (flavor, marker)
                data = data.replace(marker, marker[:1] + value.to_bytes(2, 'little'))
            cmd.write_bytes(data)

        count = 0
        for size in (loaded + 256, 16384, 65536):
            allocate(size)
            for records in (0, 1, 513):
                payload = (bytes(range(256)) * ((records + 1) // 2))[:records * 128]
                for collision in (False, True):
                    setup(payload, collision)
                    output = execute('wport', 'alpha.dat', input=b'CBYX' if collision else b'CBX', timeout=10)
                    assert b'Directory too large' not in output, (flavor, size, output)
                    assert b'GUARD FAILED' not in output, (flavor, size, output)
                    assert inventory(a) == inventory(b) == {'alpha.dat': payload}, (flavor, size, records, output)
                    count += 1
        # A directory larger than the small allocation must stop before writing
        # past the group, without touching source files or creating destinations.
        allocate(loaded + 256)
        setup(b'A' * 128)
        for number in range(80):
            (a / f'f{number:03}.dat').write_bytes(b'B' * 128)
        before = inventory(a)
        output = execute('wport', '*.dat', input=b'X', timeout=10)
        assert b'Directory too large for memory' in output, (flavor, output)
        assert b'GUARD FAILED' not in output, (flavor, output)
        assert inventory(a) == before and inventory(b) == {}
        report.append(f'| {flavor} | {count} exact-byte cases passed | overflow rejected; files unchanged |')
        print(flavor, count, 'native allocation copy/replace cases and directory bound passed', flush=True)
assert hashlib.sha256(source.read_bytes()).hexdigest() == digest

if len(sys.argv) > 1:
    Path(sys.argv[1]).write_text('\n'.join(report) + '\n')
