#!/usr/bin/env python3
"""Exercise editable symbol state through the public CLI and NASM."""
import json
import os
from pathlib import Path
import subprocess
import tempfile

exe = Path(os.environ.get('XLT88', Path(__file__).resolve().parent.parent / 'xlt88'))

with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    source = root / 'input.asm'
    state = root / 'names.sym'
    output = root / 'out.asm'
    source.write_text("bdos equ 5\nprint equ 9\nstart: lxi d,msg\n mvi c,print\n call bdos\n jmp 0\nmsg: db 'msg$'\n end\n")

    def run(*options, ok=True, dest=output, src=source):
        result = subprocess.run([str(exe), *options, str(src), str(dest)], capture_output=True)
        assert (result.returncode == 0) == ok, result.stderr.decode()

    def assemble(path):
        binary = root / 'out.bin'
        subprocess.run(['nasm', '-f', 'bin', str(path), '-o', str(binary)], check=True)
        return binary.read_bytes()

    run('--state-out=' + str(state))
    baseline = assemble(output)
    original = json.loads(state.read_text())
    edited = json.loads(state.read_text())
    for symbol in edited['symbols']:
        symbol['preferred_name'] = 'named_' + symbol['original_name']
    state.write_text(json.dumps(edited))
    run('--state-in=' + str(state), '--state-out=' + str(state))
    assert json.loads(state.read_text()) == edited
    assert assemble(output) == baseline
    text = output.read_text()
    assert 'named_msg' in text and "'msg$'" in text
    run('--state-in=' + str(state), '--emit=8080')
    text = output.read_text()
    assert 'named_start:' in text and 'named_msg' in text and "'msg$'" in text
    normalized = root / 'norm.asm'
    normalized.write_text(text)
    run(src=normalized)
    assert assemble(output) == baseline
    run('--target=cpm86')
    cpm_baseline = assemble(output)
    run('--target=cpm86', '--state-in=' + str(state))
    assert assemble(output) == cpm_baseline
    # A symbol called A must not rename the register A in MOV/MVI operands.
    registers = root / 'regs.asm'
    registers.write_text("a equ 1\n mvi a,a\n mov b,a\n jmp 0\n end\n")
    regstate = root / 'regs.sym'
    run('--state-out=' + str(regstate), src=registers)
    register_baseline = assemble(output)
    names = json.loads(regstate.read_text())
    names['symbols'][0]['preferred_name'] = 'constant'
    regstate.write_text(json.dumps(names))
    run('--state-in=' + str(regstate), '--emit=8080', src=registers)
    normalized.write_text(output.read_text())
    run(src=normalized)
    assert assemble(output) == register_baseline
    # Reject stale input, duplicate names, changed EQU values, unknown schema,
    # invalid names, malformed JSON, duplicate fields and non-null addresses.
    malformed = ['{', '{}', '[]', 'null', 'true', '[' * 18 + ']' * 18, state.read_text() + 'x', '{"schema_version":1,"schema_version":1}']
    for field, value in [('schema_version', 2), ('source_fingerprint', 'stale')]:
        bad = json.loads(json.dumps(edited))
        bad[field] = value
        malformed.append(json.dumps(bad))
    for field, value in [('preferred_name', 'bad-name'), ('preferred_name', 'a'),
                         ('equ_expression', '6'), ('equ_value', 6),
                         ('source_address', 256), ('id', 99)]:
        bad = json.loads(json.dumps(edited))
        bad['symbols'][0][field] = value
        malformed.append(json.dumps(bad))
    bad = json.loads(json.dumps(edited))
    bad['symbols'][1]['preferred_name'] = bad['symbols'][0]['preferred_name'].upper()
    malformed.append(json.dumps(bad))
    for text in malformed:
        state.write_text(text)
        output.write_text('preserve')
        run('--state-in=' + str(state), ok=False)
        assert output.read_text() == 'preserve'
        assert state.read_text() == text
    state.write_text(json.dumps(edited))
    source.write_text(source.read_text().replace("'msg$'", "'changed$'"))
    run('--state-in=' + str(state), ok=False)
    # Output/state aliases never overwrite the source or state input.
    for option in ['--state-in=', '--state-out=']:
        run(option + str(source), ok=False)
        run(option + str(output), ok=False)
    alias = root / 'alias.sym'
    alias.hardlink_to(source)
    run('--state-out=' + str(alias), ok=False)
    alias.unlink()
    alias.symlink_to(source)
    run('--state-out=' + str(alias), ok=False)
    missing = root / 'new.asm'
    run('--state-out=' + str(root) + '/./new.asm', dest=missing, ok=False)
    assert not missing.exists()
    assert not list(root.glob('xl??????'))
print('Symbol state: round trips, renaming, binary equivalence, rejection and path protection passed')
