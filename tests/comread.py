"""Annotated COM decoding, symbol-state reuse and native execution."""
import json
import re
from itertools import product
from pathlib import Path
import subprocess
import tempfile
from toolrun import exe, runner


with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    source, mapping, output = work/'input.com', work/'input.map', work/'out.asm'
    # LXI D,message; MVI C,9; CALL 5; JMP 0, followed by literal bytes.
    binary = bytes.fromhex('110b010e09cd0500c30000') + b'COM OK$'
    annotations = 'code 100 10b\ndata 10b 112\nptr 100\n'

    # CLI contract failures explain the missing/inapplicable option before I/O.
    for options, message in (
        (['--dialect=com'], b'COM input requires --com-map='),
        (['--com-map='+str(mapping)], b'--com-map is only valid with --dialect=com'),
    ):
        output.write_text('preserve')
        result = subprocess.run([str(exe), *options, str(source), str(output)], capture_output=True)
        assert result.returncode == 1 and message in result.stderr, result
        assert output.read_text() == 'preserve'

    def run(data=binary, layout=annotations, *options, ok=True):
        source.write_bytes(data)
        mapping.write_text(layout)
        output.write_text('preserve')
        result = subprocess.run([str(exe), '--dialect=com', '--com-map='+str(mapping),
                                 *options, str(source), str(output)], capture_output=True)
        assert (result.returncode == 0) == ok, result.stderr.decode()
        if not ok:
            assert output.read_text() == 'preserve'
        return result

    run(binary, annotations, '--emit=8080')
    normalized = output.read_bytes()
    assert b'lxi\td,l010b' in normalized, normalized
    state = work/'names.sym'
    run(binary, annotations, '--emit=8080', '--state-out='+str(state))
    names = json.loads(state.read_text())
    message = next(s for s in names['symbols'] if s['source_address'] == 0x10b)
    message['preferred_name'] = 'message'
    state.write_text(json.dumps(names))
    run(binary, annotations, '--emit=8080', '--state-in='+str(state))
    assert b'lxi\td,message' in output.read_bytes()
    # A changed pointer interpretation changes the state identity.
    run(binary, annotations.replace('ptr 100\n', ''), '--state-in='+str(state), ok=False)

    # All documented opcode bytes decode; the twelve undocumented aliases do
    # not. Give each instruction enough trailing bytes without guessing data.
    aliases = {0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38, 0xcb, 0xd9, 0xdd, 0xed, 0xfd}
    low = '''nop lxi stax inx inr dcr mvi rlc - dad ldax dcx inr dcr mvi rrc
             - lxi stax inx inr dcr mvi ral - dad ldax dcx inr dcr mvi rar
             - lxi shld inx inr dcr mvi daa - dad lhld dcx inr dcr mvi cma
             - lxi sta inx inr dcr mvi stc - dad lda dcx inr dcr mvi cmc'''.split()
    high = '''rnz pop jnz jmp cnz push adi rst rz ret jz - cz call aci rst
              rnc pop jnc out cnc push sui rst rc - jc in cc - sbi rst
              rpo pop jpo xthl cpo push ani rst rpe pchl jpe xchg cpe - xri rst
              rp pop jp di cp push ori rst rm sphl jm ei cm - cpi rst'''.split()
    expected = low + ['hlt' if op == 0x76 else 'mov' for op in range(0x40, 0x80)]
    expected += [name for name in ('add', 'adc', 'sub', 'sbb', 'ana', 'xra', 'ora', 'cmp') for _ in range(8)]
    expected += high
    count = 0
    for opcode in range(256):
        run(bytes((opcode, 0, 0)), 'code 100 103\n', '--emit=8080', ok=opcode not in aliases)
        if opcode not in aliases:
            assert re.search(r'l0100:\s+'+expected[opcode]+r'\b', output.read_text()), (opcode, output.read_text())
        count += opcode not in aliases
    assert count == 244

    # Annotated pointer words and explicit LXI pointers share normal labels.
    run(bytes.fromhex('210401e90401'), 'code 100 104\nword 104 106\nptr 100\n', '--emit=8080')
    assert b'dw\tl0104' in output.read_bytes()
    # Explicit one-past pointers are useful for stack/buffer limits. Ordinary
    # LXI constants remain numeric even when their value lies inside the image.
    run(bytes.fromhex('210301'), 'code 100 103\nptr 100', '--emit=8080')
    assert b'lxi\th,l0103' in output.read_bytes()
    run(bytes.fromhex('210301'), 'code 100 103', '--emit=8080')
    assert b'lxi\th,259' in output.read_bytes()

    for data, layout in [
        (b'', ''), (bytes(65281), 'code 100 101\n'),
        (b'\x21\x00', 'code 100 102\n'),
        (binary, 'code 100 10b\n'),
        (binary, annotations+'data 100 101\n'),
        (binary, annotations+'ptr 101\n'),
        (binary, annotations+'ptr 103 junk\n'),
        (binary, annotations+'ptr 100\n'),
        (binary, annotations+'#'+('x'*256)+'\n'),
        (binary, annotations+'ptr 103\x00ignored'),
        (binary, annotations.replace('112', '1000000000000')),
        (bytes.fromhex('c30101'), 'code 100 103\n'),
        (bytes.fromhex('210001'), 'code 100 101\ndata 101 103\n'),
        (b'\x00\x00', 'data 100 102\n'),
        (b'\x00\x00', 'code 100 101\nword 101 102\n'),
    ]:
        run(data, layout, '--emit=8080', ok=False)

    run()
    # Map input, including hardlink aliases, must not be replaced by output.
    alias = work/'alias.map'
    alias.hardlink_to(mapping)
    result = subprocess.run([str(exe), '--dialect=com', '--com-map='+str(mapping),
                             str(source), str(alias)], capture_output=True)
    assert result.returncode and mapping.read_text() == annotations

    for target, flavor in product(('msdos', 'cpm86'), ('nasm', 'asm86', 'masm3', 'drasm86')):
        _, build, execute = runner(work, target)
        mapping.write_text(annotations)
        build('comtest', binary, flavor, dialect='com', com_map=mapping)
        assert b'COM OK' in execute('comtest'), (target, flavor)
        build('flat', normalized, flavor)
        assert execute('flat') == execute('comtest'), (target, flavor)
        print(target, flavor, 'annotated COM and normalized source execute identically', flush=True)
    print('COM: 244 documented opcodes, 12 alias rejections, maps/state/bounds and eight native paths passed')
