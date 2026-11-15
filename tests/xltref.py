"""Fresh XLT86 comparison support shared by the application benchmarks."""
import hashlib
import os
import re
from collections import Counter
from toolrun import exe, assembler, gencmd, runner, cmd_span


def legacy_text(text):
    return text.replace('\r\n', '\n').rstrip('\x1a').replace('\n', '\r\n').encode() + b'\x1a'


def compare_psw(work, report):
    """A focused subtraction probe, not a claim of full XLT86 conformance."""
    from arith import fixture, reference
    command, build, execute = runner(work, 'cpm86')
    text = fixture('sub', 'immediate', [(0x10, 1, 2)])
    expected = reference('sub', 0x10, 1, 2)
    (work/'psw.asm').write_bytes(legacy_text(text))
    log = command([os.environ.get('XLT86', 'cpm_xlt86'), 'psw'])
    assert b'Translate-86' in log and b'Number of Errors:' not in log, log
    log = command([assembler, 'psw'])
    assert b'NUMBER OF ERRORS:   0' in log and b'ERROR NO:' not in log, log
    command([gencmd, 'psw', '8080'])
    result = execute('psw').strip().decode('ascii')
    assert result == 'OK' or re.fullmatch(r'0000[0-9A-F]{4}', result), result
    build('psw88', text, 'asm86')
    assert execute('psw88').strip() == b'OK'
    actual = expected if result == 'OK' else (int(result[4:6], 16), int(result[6:8], 16))
    report += [f'Flag probe (A=10h, SUI 1, PUSH PSW): reference model/result '
               f'{expected[0]:02X}/{expected[1]:02X}; fresh XLT86 {actual[0]:02X}/{actual[1]:02X}; '
               'xlt88 matches the reference model. This checks one case, not all flag behaviour.', '']


def prepare_disk(text):
    """Syntax-only preparation, applied identically to both translators."""
    names = {'ESC': 'ESCCHR', 'LAST': 'LASTADR', 'LIST': 'LISTDEV',
             'LOOP': 'DLOOP', 'TYPE': 'TYPECHR', 'CLS': 'CLRSCR',
             '__MAC_SET_1': 'RINGBASE'}
    text = re.sub(r"'(?:[^']|'')*'|;[^\n]*|[A-Za-z@?$_][A-Za-z0-9@?$_]*",
                  lambda m: names.get(m[0].upper(), m[0]), text)
    # MAC labels on conditional directives become location EQU records.
    # XLT86 needs a code label for a CALL, not an untyped numeric equate.
    text = re.sub(r'^CLRSCR\s+equ\s+\$ AND 65535\s*$', 'CLRSCR:', text, flags=re.M)
    lines, deferred = [], None
    for line in text.splitlines():
        if line.startswith('RINGBASE\t'):
            deferred = line
            continue
        if re.match(r'\s+end\b', line, re.I):
            assert deferred is not None
            lines += [deferred, '\tend']
            continue
        match = re.match(r'^(\s*(?:[A-Za-z@?$_][A-Za-z0-9@?$_]*:\s*)?)db\s+(.*)$', line, re.I)
        if not match:
            lines.append(line)
            continue
        # Quote-aware tokens keep commas/semicolons inside strings intact.
        items = re.findall(r"\s*('(?:[^']|'')*'|[^,;]+|;.*)", match[2])
        first = True
        for item in items:
            item = item.strip()
            if not item or item.startswith(';'):
                continue
            if item.startswith("'"):
                chars = re.findall(r"''|[^']", item[1:-1])
                parts = ["'"+''.join(chars[i:i+32])+"'" for i in range(0, len(chars), 32)] or ["''"]
            else:
                parts = [item]
            for part in parts:
                lines.append((match[1] if first else '\t')+'db\t'+part)
                first = False
    return '\n'.join(lines)+'\n'


def port_xlt(text, program):
    """Explicit reference-port edits; never applied to production translation."""
    if program == 'wash':
        assert text.count('\tCLI\n') == 1
        text = text.replace('\tCLI\n', '\tCLI\n\tMOV AX,DS\n\tMOV SS,AX\n')
        text, n = re.subn(r'(\tMOV\s+SP,.*\n)', r'\1\tSTI\n', text, count=1)
        assert n == 1
        for old, new in [('MOV\tAL,Byte Ptr PNAME', 'MOV DI,(Offset PNAME)\n\tMOV AL,[DI]'),
                         ('MOV\tByte Ptr PNAME,AL', 'MOV [DI],AL')]:
            assert text.count(old) == 1
            text = text.replace(old, new)
    elif program == 'disk':
        text, n = re.subn(r'(\tMOV\s+SP,\(Offset STACK\)[^\n]*\n)',
                         r'\tMOV AX,DS\n\tCLI\n\tMOV SS,AX\n\1\tSTI\n', text, count=1)
        assert n == 1
        first, last = text.index('FRESTOR:'), text.index('FREE20:')
        block = text[first:last]
        block, n = re.subn(r'(MOV\s+(?:AL|DL|DH),)M\b', r'\1ES:M', block, count=4)
        assert n == 4
        text = text[:first]+block+text[last:]
        first, last = text.index('GSPBYT:'), text.index('GSPLUP:')
        block = text[first:last]
        assert block.count('[SI]') == 1
        text = text[:first]+block.replace('[SI]', 'ES:[SI]')+text[last:]
    return text


def listing_size(path, code_bytes=None):
    """Count object bytes, not source lines; retain data continuations.

    Reserve counts use listing address deltas. They include trailing BSS,
    unlike the rounded load span. ORG gaps are not instruction bytes.
    """
    counts = Counter(code=0, data=0, reserve=0)
    reserves = []
    ops = Counter()
    category, pending = 'code', None
    for line in path.read_text(errors='replace').split('\x1a')[0].splitlines():
        address = re.match(r'^ ([0-9A-F]{4})\s', line)
        if not address and '$' in line[23:]:
            address = re.match(r'^   ([0-9A-F]{4})\s', line)
        if address and pending is not None:
            end = int(address[1], 16)
            assert end >= pending, (path, line, pending)
            counts['reserve'] += end-pending
            reserves.append((pending, end))
            pending = None
        source = line[23:].split(';', 1)[0].strip() if len(line) > 23 else ''
        tokens = source.upper().split()
        directive = next((t for t in tokens[:2] if t in ('DB', 'DW', 'DD', 'RS', 'RW', 'RB')), None)
        if directive in ('RS', 'RW', 'RB') and address:
            operand = re.split(r'\b'+directive+r'\b', source.upper(), maxsplit=1)[1].strip()
            if '$' in operand:
                pass  # xlt88 expresses fixed ORG padding as RB (origin-$).
            elif operand.isdecimal():
                size = int(operand) * (2 if directive == 'RW' else 1)
                counts['reserve'] += size
                start = int(address[1], 16)
                reserves.append((start, start+size))
            else:
                pending = int(address[1], 16)
        obj = re.match(r'^ [0-9A-F]{4} ([0-9A-F]+)(?:\s|$)', line)
        continuation = re.fullmatch(r' {6}([0-9A-F]+)\s*', line)
        if obj:
            category = 'data' if directive in ('DB', 'DW', 'DD') else 'code'
            if category == 'code':
                # ASM86 may print LABEL:MOV without intervening whitespace.
                mnemonic = re.sub(r'^\S+:\s*', '', source).split()[0].upper()
                ops[mnemonic] += 1
                if code_bytes is not None:
                    code_bytes[mnemonic] += len(obj[1])//2
        if obj or continuation:
            encoded = (obj or continuation)[1]
            assert len(encoded) % 2 == 0
            counts[category] += len(encoded)//2
    assert pending is None, (path, 'unmeasured final reserve')
    initialized, last = 0, 0x100
    for line in path.with_suffix('.h86').read_text().split('\x1a')[0].splitlines():
        if line.startswith(':'):
            record = bytes.fromhex(line[1:])
            assert len(record) == record[0]+5 and sum(record) % 256 == 0
            if record[3] in (0x81, 0x82):
                initialized += record[0]
                last = max(last, int.from_bytes(record[1:3], 'big')+record[0])
    assert counts['code']+counts['data'] == initialized, (path, counts, initialized)
    loaded_reserve = sum(max(0, min(end, last)-max(start, 0x100)) for start, end in reserves)
    counts['gap'] = last-0x100-initialized-loaded_reserve
    assert counts['gap'] >= 0, (path, counts)
    return counts, ops


def fresh_compare(work, source, dialect, program, report, native_port=None):
    """Return fresh/ported baseline and xlt88 names for runtime comparison."""
    command, build, _ = runner(work, 'cpm86')
    command([str(exe), '--dialect='+dialect, '--emit=asm80', str(source), 'common.asm'])
    text = (work/'common.asm').read_text().rstrip('\x1a')
    preparation = 'normalized ASM80 only'
    if program == 'disk':
        text = prepare_disk(text)
        preparation += '; rename seven symbols, restore CLRSCR code label, defer RINGBASE EQU, omit END operand, split DB items/strings'
    common = legacy_text(text)
    (work/'common.asm').write_bytes(common)
    log = command([os.environ.get('XLT86', 'cpm_xlt86'), 'common'], timeout=60)
    assert b'Translate-86' in log and b'Number of Errors:' not in log, log
    version = re.search(rb'XLT86 Version [^\r\n]+', log)
    assert version and (work/'common.asm').read_bytes() == common
    version = version[0].decode()
    # Preserve the raw translation even when assembly requires explicit edits.
    raw = (work/'common.a86').read_bytes()
    log = command([assembler, 'common'], codes=(0, 1))
    errors = re.search(rb'NUMBER OF ERRORS:\s+(\d+)', log)
    assert errors, log
    expected = 2 if program == 'wash' else 0
    assert int(errors[1]) == expected, log
    report += ['', '## Fresh XLT86 / identical-input comparison', '',
               f'Translator: {version}. Preparation: {preparation}.',
               f'Common ASM80 SHA256: `{hashlib.sha256(common).hexdigest()}`.',
               f'Raw XLT86 SHA256: `{hashlib.sha256(raw).hexdigest()}`; ASM86 errors: {int(errors[1])}.',
               'Both translators receive exactly the same prepared bytes. Raw output is not the supplied patched reference.', '']
    if not expected:
        command([gencmd, 'common', '8080'])
    patched = port_xlt(raw.decode().replace('\r\n', '\n').rstrip('\x1a'), program)
    (work/'fresh.a86').write_bytes(legacy_text(patched))
    log = command([assembler, 'fresh'])
    assert b'NUMBER OF ERRORS:   0' in log and b'ERROR NO:' not in log, log
    command([gencmd, 'fresh', '8080'])
    build('same', common, 'asm86')
    rows = [('fresh XLT86 + explicit port' if program != 'vtl' else 'fresh XLT86', 'fresh'), ('xlt88 identical input', 'same')]
    if native_port is not None:
        build('sameport', common, 'asm86', patch=native_port)
        rows.append(('xlt88 + explicit port', 'sameport'))
    if not expected and program != 'vtl':
        rows.insert(0, ('raw XLT86', 'common'))
    report += ['| Build | Instruction bytes | Initialized data | Reserved bytes | ORG gaps | Load span |',
               '| --- | ---: | ---: | ---: | ---: | ---: |']
    stats, byte_stats = {}, {}
    for label, name in rows:
        byte_stats[name] = Counter()
        count, ops = listing_size(work/(name+'.lst'), byte_stats[name])
        assert sum(byte_stats[name].values()) == count['code']
        span = cmd_span(work/(name+'.cmd'))
        report.append(f'| {label} | {count["code"]} | {count["data"]} | {count["reserve"]} | {count["gap"]} | {span} |')
        stats[name] = ops
    report += ['', 'Reserved bytes include trailing BSS; load span includes holes and paragraph padding, so these columns are not additive.',
               'Explicit XLT86 port: '+({'vtl':'none', 'wash':'SS/interrupt setup and two PNAME accesses via DI',
                                       'disk':'SS/interrupt setup and five ES reads'}[program])+'.',
               'Instruction counts (fresh baseline / xlt88): '+', '.join(
                   f'{op} {stats["fresh"][op]}/{stats["same"][op]}'
                   for op in ('MOV','LAHF','SAHF','PUSH','POP','XCHG','NOP','INT'))+'.', '']
    # Byte accounting complements textual inspection; these are instruction
    # families, not claims that every byte in a family has the same purpose.
    report += ['| Instruction family | XLT86 bytes | xlt88 bytes | Difference |',
               '| --- | ---: | ---: | ---: |']
    groups = [('Flag image', {'LAHF', 'SAHF'}), ('XOR', {'XOR'}),
              ('AND/OR', {'AND', 'OR'}), ('Register stack', {'PUSH', 'POP'}),
              ('MOV', {'MOV'}), ('XCHG', {'XCHG'}),
              ('ADD/shift', {'ADD', 'SHL'}), ('Other', set())]
    remaining = set(byte_stats['fresh']) | set(byte_stats['same'])
    for label, mnemonics in groups:
        selected = mnemonics if label != 'Other' else remaining
        a, b = (sum(byte_stats[name][op] for op in selected) for name in ('fresh', 'same'))
        report.append(f'| {label} | {a} | {b} | {b-a:+d} |')
        remaining -= selected
    report += ['', 'Families include OS adapter instructions; raw xlt88 precedes any explicit ES port.', '']
    return 'fresh', 'sameport' if native_port is not None else 'same'
