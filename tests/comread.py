"""Annotated COM decoding, symbol-state reuse and native execution."""
import json
import re
import os
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

    # Large unresolved regions must not create a label/record per byte and
    # overwhelm native analyses. Preserve exact bytes and referenced boundaries.
    payload = bytes(range(256))*96
    source.write_bytes(bytes.fromhex('3a1101c9') + payload)
    recovered = work/'large.asm'
    result = subprocess.run([str(exe), '--dialect=com', '--emit=asm80',
                             str(source), str(recovered)], capture_output=True, timeout=10)
    assert result.returncode == 2, result.stderr
    listing = recovered.read_text()
    assert 'l0111:' in listing and len(listing.splitlines()) < 2000
    items = re.findall(r"'([^']*)'|([0-9]+)", '\n'.join(
        re.findall(r'\bdb\s+([^\r\n]+)', listing)))
    assert b''.join(text.encode('ascii') if text else bytes([int(number)])
                    for text, number in items) == payload
    assert "'" in listing  # Printable runs do not change unknown classification.
    assert 'unknown' in (work/'large.map').read_text()
    native = work/'large86.a86'
    result = subprocess.run([str(exe), '--dialect=com', str(source), str(native)],
                            capture_output=True, timeout=10)
    assert result.returncode == 1 and not native.exists(), result.stderr
    assert b'native output was not written' in result.stderr
    # A reviewed large data region still exercises native lowering performance.
    layout = work/'large.map'
    layout.write_text(f'code 0100 0104\ndata 0104 {0x104+len(payload):04x}\n')
    result = subprocess.run([str(exe), '--dialect=com', '--com-map='+str(layout),
                             str(source), str(native)], capture_output=True, timeout=10)
    assert result.returncode == 0, result.stderr
    # CLI contract failures explain an inapplicable option before I/O.
    for options, message in (
        (['--com-map='+str(mapping)], b'--com-map is only valid with --dialect=com'),
    ):
        output.write_text('preserve')
        result = subprocess.run([str(exe), *options, str(source), str(output)], capture_output=True)
        assert result.returncode == 1 and message in result.stderr, result
        assert output.read_text() == 'preserve'

    # Discovery uses the normal selected emitter; no companion assembly file.
    source.write_bytes(binary)
    draft = work/'draft.asm'
    result = subprocess.run([str(exe), '--dialect=com', '--emit=asm80', str(source), str(draft)], capture_output=True)
    assert result.returncode == 2 and draft.exists(), result.stderr
    assert not list(work.glob("*.80"))
    draft_map, review = work/'draft.map', draft
    assert 'code 0100 010b' in draft_map.read_text()
    assert 'unknown 010b 0112' in draft_map.read_text()
    assert b'; XLT88 TODO:' in review.read_bytes() and b'LXI used as address' in review.read_bytes()
    assert review.read_bytes().endswith(b'\r\n\x1a')
    before = draft_map.read_bytes(), review.read_bytes()
    result = subprocess.run([str(exe), '--dialect=com', '--emit=asm80', str(source), str(draft)], capture_output=True)
    assert result.returncode == 1 and before == (draft_map.read_bytes(), review.read_bytes())
    result = subprocess.run([str(exe), '--dialect=com', '--com-map='+str(draft_map), str(source), str(draft)], capture_output=True)
    assert result.returncode == 1 and b'unresolved unknown' in result.stderr
    # Refining the generated map is sufficient to enter the normal pipeline.
    draft_map.write_text(draft_map.read_text().replace('unknown', 'data'))
    result = subprocess.run([str(exe), '--dialect=com', '--com-map='+str(draft_map), '--emit=asm80', str(source), str(draft)], capture_output=True)
    assert result.returncode == 0 and b'lxi\td,l010b' in draft.read_bytes(), result.stderr

    # Unreviewed recovery must not masquerade as native assembly.
    source.write_bytes(binary)
    for flavor in ('nasm', 'asm86', 'masm3', 'drasm86'):
        native = work/(flavor+'.a86')
        result = subprocess.run([str(exe), '--dialect=com', '--target=cpm86',
                                 '--emit='+flavor, str(source), str(native)], capture_output=True)
        assert result.returncode == 1 and not native.exists(), result.stderr
        assert b'native output was not written' in result.stderr
        assert native.with_suffix('.map').exists()
        assert not native.with_suffix('.80').exists()

    # Follow both branch paths and calls, but not indirect or invalid entries.
    for name, data, note in (
        ('branch', bytes.fromhex('ca0701cd0801c9c9c9'), None),
        ('indirect', bytes.fromhex('e900'), b'PCHL destination unknown'),
        ('invalid', bytes.fromhex('dd00'), b'invalid or truncated'),
        ('overlap', bytes.fromhex('c30101'), b'control flow enters an instruction operand'),
        ('short', bytes.fromhex('21ff'), b'invalid or truncated'),
    ):
        source.write_bytes(data)
        listing = work/(name+'.asm')
        result = subprocess.run([str(exe), '--dialect=com', '--emit=asm80', str(source), str(listing)], capture_output=True)
        assert result.returncode == 2, result.stderr
        if note:
            assert note in listing.read_bytes(), listing.read_bytes()
        else:
            assert 'code 0100 0109' in listing.with_suffix('.map').read_text()

    # Inline argument recovery follows instruction behavior, independently of
    # helper address. The invalid byte is data, not a failed decode after CALL.
    for address in (0x108, 0x180):
        data = bytearray(bytes.fromhex('cd0001ddc30000'))
        data[1:3] = address.to_bytes(2, 'little')
        data.extend(bytes(address - 0x100 - len(data)))
        data.extend(bytes.fromhex('e37e23e3c9'))
        source.write_bytes(data)
        listing = work/f'inl{address:x}.asm'
        result = subprocess.run([str(exe), '--dialect=com', '--emit=asm80',
                                 str(source), str(listing)], capture_output=True, timeout=10)
        assert result.returncode == 2, result.stderr
        assert 'data 0103 0104' in listing.with_suffix('.map').read_text()
        assert 'invalid or truncated' not in listing.read_text()

    # Conflicting return adjustments cannot establish an inline-data length.
    source.write_bytes(bytes.fromhex('cd0801ddc9000000e3ca100123e3c900e3c9'))
    listing = work/'conflict.asm'
    result = subprocess.run([str(exe), '--dialect=com', '--emit=asm80',
                             str(source), str(listing)], capture_output=True, timeout=10)
    assert result.returncode == 2, result.stderr
    assert 'data 0103' not in listing.with_suffix('.map').read_text()
    assert 'COM 0103: reachable invalid' in listing.read_text()

    # A helper reads an inline target, advances the saved return and uses PCHL.
    data = bytearray(bytes.fromhex('cd08014001c30000e15e235623e5ebe9'))
    data.extend(bytes(0x140-0x100-len(data)))
    data.append(0xc9)
    source.write_bytes(data)
    listing = work/'dispatch.asm'
    result = subprocess.run([str(exe), '--dialect=com', '--emit=asm80',
                             str(source), str(listing)], capture_output=True, timeout=10)
    assert result.returncode == 2, result.stderr
    assert 'data 0103 0105' in listing.with_suffix('.map').read_text()
    assert re.search(r'l0140:\s+ret', listing.read_text())

    # A stack-loaded entry survives a tail jump; resetting SP discards old hints.
    for name, data, expected in (
        ('retentry', '211001444d310080c5c30c01c9000000c9', True),
        ('resetsp', '211001e5310080c90000000000000000c9', False),
    ):
        source.write_bytes(bytes.fromhex(data))
        listing = work/(name+'.asm')
        result = subprocess.run([str(exe), '--dialect=com', '--emit=asm80',
                                 str(source), str(listing)], capture_output=True, timeout=10)
        assert result.returncode == 2, result.stderr
        assert bool(re.search(r'l0110:\s+ret', listing.read_text())) == expected

    # Address use, not an in-image number alone, supplies ptr candidates.
    for name, data, pointers in (
        ('direct', '2107017ec300002a', {0x100}),
        ('replace', '210a01210a017ec300002a', {0x103}),
        ('integer', '11080121000019c92a', set()),
        ('callee', '210b01cd0901c300007ec92a', {0x100}),
        ('saved', '111001d5cd0e01d11ac300000000afc92a', {0x100}),
        ('halves', '21090155621ac300002a', set()),
    ):
        source.write_bytes(bytes.fromhex(data))
        listing = work/(name+'.asm')
        result = subprocess.run([str(exe), '--dialect=com', '--emit=asm80',
                                 str(source), str(listing)], capture_output=True, timeout=10)
        assert result.returncode == 2, result.stderr
        actual = {int(address, 16) for address in re.findall(r'^ptr ([0-9a-f]+)$',
                  listing.with_suffix('.map').read_text(), re.M)}
        assert actual == pointers, (name, actual, pointers)

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

    # An explicit indirect word use is evidence; a numeric resemblance is not.
    for name, data, found in (
        ('indword', bytes.fromhex('2a0401e90601c9'), True),
        ('wordnum', bytes.fromhex('2a0401c90601c9'), False),
        ('wordout', bytes.fromhex('2a0401e90080c9'), False),
        ('wordself', bytes.fromhex('2a0401e90401c9'), False),
    ):
        source.write_bytes(data)
        recovered = work/(name+'.asm')
        result = subprocess.run([str(exe), '--dialect=com', '--emit=asm80',
                                 str(source), str(recovered)], capture_output=True)
        assert result.returncode == 2, result.stderr
        layout = recovered.with_suffix('.map').read_text()
        assert ('word 0104 0106' in layout) == found, layout
        if found:
            assert 'code 0106 0107' in layout and 'review runtime writes' in layout
            mapping.write_text(layout)
            result = subprocess.run([str(exe), '--dialect=com', '--com-map='+str(mapping),
                                     '--emit=asm80', str(source), str(output)], capture_output=True)
            assert result.returncode == 0 and b'dw\tl0106' in output.read_bytes(), result.stderr

    # A proven indirect slot can receive additional literal entry addresses.
    source.write_bytes(bytes.fromhex('211101220c012a0c01e900000e01c30000c30000'))
    recovered = work/'stored.asm'
    result = subprocess.run([str(exe), '--dialect=com', '--emit=asm80',
                             str(source), str(recovered)], capture_output=True)
    assert result.returncode == 2, result.stderr
    layout = recovered.with_suffix('.map').read_text()
    assert 'word 010c 010e' in layout and 'code 010e 0114' in layout, layout
    assert 'ptr 0100' in layout and 'indirect pointer at 010c is writable' in layout
    run(source.read_bytes(), layout.replace('unknown ', 'data '), '--emit=asm80')
    assert b'lxi\th,l0111' in output.read_bytes()
    assert b'indirect pointer at 010c is writable' in output.read_bytes()

    # Port hazards survive reviewed-map input and reach native generated TODOs.
    for name, data, layout, message in (
        ('opcode', '3e00320001c30000', 'code 100 108\n', b'store at 0100 overlaps decoded code'),
        ('operand', '210000220101c30000', 'code 100 109\n', b'store at 0101 overlaps decoded code'),
        ('bios', '2a0100c30000', 'code 100 106\n', b'reads CP/M-80 vector at 0001'),
        ('top', '2a0600c30000', 'code 100 106\n', b'reads CP/M-80 vector at 0006'),
    ):
        if name == 'operand':
            run(bytes.fromhex(data), layout, '--emit=asm80', ok=False)
            recovered = work/'hazard.asm'
            result = subprocess.run([str(exe), '--dialect=com', '--emit=asm80',
                                     str(source), str(recovered)], capture_output=True)
            assert result.returncode == 2 and message in recovered.read_bytes()
            continue
        run(bytes.fromhex(data), layout, '--emit=asm80')
        assert message in output.read_bytes(), (name, output.read_bytes())
        # The native translator may reject code-operand aliases itself; for
        # valid instruction boundaries the port TODO must remain visible.
        if name != 'operand':
            run(bytes.fromhex(data), layout, '--target=cpm86', '--emit=nasm')
            assert message in output.read_bytes(), (name, output.read_bytes())

    # Text formatting keeps quotes, controls, high bytes and short runs numeric.
    text_bytes = b"ABCD'EFGH;I,J" + bytes((0, 13, 10, 26, 127, 128, 255)) + b'XYZ'
    run(b'\xc9'+text_bytes, f'code 100 101\ndata 101 {0x101+len(text_bytes):x}\n', '--emit=asm80')
    rendered = output.read_text()
    assert "'ABCD',39,'EFGH;I,J'" in rendered, rendered
    assert '127,128,255,88,89,90' in rendered, rendered

    run(binary, annotations, '--emit=asm80')
    normalized = output.read_bytes()
    assert b'lxi\td,l010b' in normalized, normalized
    state = work/'names.sym'
    run(binary, annotations, '--emit=asm80', '--state-out='+str(state))
    names = json.loads(state.read_text())
    message = next(s for s in names['symbols'] if s['source_address'] == 0x10b)
    message['preferred_name'] = 'message'
    state.write_text(json.dumps(names))
    run(binary, annotations, '--emit=asm80', '--state-in='+str(state))
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
        run(bytes((opcode, 0, 0)), 'code 100 103\n', '--emit=asm80', ok=opcode not in aliases)
        if opcode not in aliases:
            assert re.search(r'l0100:\s+'+expected[opcode]+r'\b', output.read_text()), (opcode, output.read_text())
        count += opcode not in aliases
    assert count == 244

    # Annotated pointer words and explicit LXI pointers share normal labels.
    run(bytes.fromhex('210401e90401'), 'code 100 104\nword 104 106\nptr 100\n', '--emit=asm80')
    assert b'dw\tl0104' in output.read_bytes()
    # Explicit one-past pointers are useful for stack/buffer limits. Ordinary
    # LXI constants remain numeric even when their value lies inside the image.
    run(bytes.fromhex('210301'), 'code 100 103\nptr 100', '--emit=asm80')
    assert b'lxi\th,l0103' in output.read_bytes()
    run(bytes.fromhex('210301'), 'code 100 103', '--emit=asm80')
    assert b'lxi\th,259' in output.read_bytes()

    # Reviewed zero padding becomes an origin, preserving page-relative data.
    padding = bytes.fromhex('c30000') + bytes(13) + b'\x7f'
    layout = 'code 100 103\ngap 103 110\ndata 110 111\n'
    run(padding, layout, '--emit=asm80')
    assert b'org\t272' in output.read_bytes()
    run(padding[:8]+b'\x01'+padding[9:], layout, '--emit=asm80', ok=False)
    run(bytes.fromhex('3a0801c9')+bytes(12)+b'\x7f',
        'code 100 104\ngap 104 110\ndata 110 111\n', '--emit=asm80', ok=False)
    # The emitted origin guard must reject expansion at assembly time.
    run(b'\x09'*8+b'\xc9'+bytes(7)+b'\x7f',
        'code 100 109\ngap 109 110\ndata 110 111\n', '--emit=nasm')
    result = subprocess.run([os.environ.get('NASM', 'nasm'), '-f', 'bin',
                             str(output), '-o', str(work/'gap.bin')], capture_output=True)
    assert result.returncode != 0 and b'TIMES value' in result.stderr, result.stderr

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
        run(data, layout, '--emit=asm80', ok=False)

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
        # The recovered word must relocate when native code expands before
        # its target; byte preservation alone cannot validate this path.
        indirect = bytes.fromhex('2a0401e906011111010e09cd0500c30000') + b'COM OK$'
        mapping.write_text('code 100 104\nword 104 106\ncode 106 111\ndata 111 118\nptr 106\n')
        build('indirect', indirect, flavor, dialect='com', com_map=mapping)
        assert execute('indirect') == execute('comtest'), (target, flavor)
        print(target, flavor, 'annotated COM and normalized source execute identically', flush=True)
    print('COM: 244 documented opcodes, 12 alias rejections, maps/state/bounds and eight native paths passed')
