#!/usr/bin/env python3
import json
import os
from pathlib import Path
import subprocess
import tempfile

exe = Path(os.environ.get('XLT88', Path(__file__).resolve().parent.parent / 'xlt88'))
with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    source, output, state = (root / name for name in ('input.asm', 'out.asm', 'names.sym'))
    text = """ org 100h
count equ 2+3
base: lxi h,data
 mvi a,1
 jmp 0108h
done: nop
data: db 'ab',0
words: dw data,done
space: ds count
 align 2
tail: ds 1
here equ $
delta equ tail-base
negative equ -1
 end
"""
    source.write_text(text)

    def run(*options, ok=True):
        result = subprocess.run([str(exe), *options, str(source), str(output)], capture_output=True)
        assert (result.returncode == 0) == ok, result.stderr.decode()
        return result.stderr.decode()

    def binary():
        destination = root / 'out.bin'
        subprocess.run(['nasm', '-f', 'bin', str(output), '-o', str(destination)], check=True)
        return destination.read_bytes()

    for target in ('msdos', 'cpm86'):
        source.write_text(text)
        run('--target=' + target, '--state-out=' + str(state))
        numeric = binary()
        data = json.loads(state.read_text())
        assert data['schema_version'] == 2
        symbols = {entry['original_name']: entry for entry in data['symbols']}
        for name, address in {'base':0x100, 'done':0x108, 'data':0x109, 'words':0x10c, 'space':0x110, 'tail':0x116}.items():
            assert symbols[name]['source_address'] == address, (name, symbols[name])
        for name, value in {'count':5, 'here':0x117, 'delta':0x16, 'negative':-1}.items():
            assert symbols[name]['equ_value'] == value, (name, symbols[name])
        assert all(entry['generated_address'] is None for entry in symbols.values())
        symbols['done']['preferred_name'] = 'finished'
        state.write_text(json.dumps(data))
        run('--target=' + target, '--state-in=' + str(state), '--state-out=' + str(state))
        assert binary() == numeric
        assert 'jmp $finished' in output.read_text()
        source.write_text(text.replace('jmp 0108h', 'jmp done'))
        run('--target=' + target)
        assert binary() == numeric
    # A v1 file with unknown addresses remains readable; export upgrades it.
    source.write_text('start: nop\n jmp 0\n end\n')
    run('--state-out=' + str(state))
    data = json.loads(state.read_text())
    data['schema_version'] = 1
    for entry in data['symbols']: entry['source_address'] = None
    state.write_text(json.dumps(data))
    run('--state-in=' + str(state), '--state-out=' + str(state))
    assert json.loads(state.read_text())['schema_version'] == 2
    # Unknown instruction size must not fabricate downstream offsets.
    source.write_text('start: nop\n mystery\nafter: nop\n end\n')
    run('--emit=8080', '--state-out=' + str(state))
    assert json.loads(state.read_text())['symbols'][1]['source_address'] is None
    # Ambiguous aliases require a named branch, rather than a guessed label.
    source.write_text('first:\nsecond: nop\n jmp 100h\n end\n')
    assert 'multiple labels' in run(ok=False)
    for source_text, message in [
        ('aa equ bb\nbb equ aa\n end\n', 'cyclic'),
        ('aa equ missing\n end\n', 'undefined'),
        (' mvi a,1/0\n end\n', 'arithmetic'),
        (' mvi a,missing\n end\n', 'undefined'),
    ]:
        source.write_text(source_text)
        output.write_text('preserve')
        assert message in run(ok=False)
        assert output.read_text() == 'preserve'
print('Source layout, numeric binding, state upgrade and diagnostics passed')
