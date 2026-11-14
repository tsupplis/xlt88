"""Compare translated execution with an 8080 arithmetic reference model."""

import os
from itertools import product
from pathlib import Path
import tempfile
from toolrun import runner

# Keep identical batches within the historical assemblers' memory limits.
BATCH = 64
EDGES = (0, 1, 7, 8, 9, 10, 15, 16, 127, 128, 153, 154, 254, 255)


def reference(op, a, b, flags):
    """Return result, S/Z/AC/P/C, including the PSW fixed bit."""
    carry = flags & 1
    if op == "daa":
        low = 6 if (a & 15) > 9 or flags & 16 else 0
        high = a + low >= 160 or carry
        value = a + low + (96 if high else 0)
        auxiliary = (a & 15) + low > 15
        carry = bool(high)
    elif op == "inr":
        value = a + 1
        auxiliary = (a & 15) == 15
    elif op == "dcr":
        value = a - 1
        auxiliary = (a & 15) != 0
    elif op == "adc":
        value = a + b + carry
        auxiliary = (a & 15) + (b & 15) + carry > 15
        carry = value > 255
    else:
        borrow = carry if op == "sbb" else 0
        value = a - b - borrow
        auxiliary = (a & 15) >= (b & 15) + borrow
        carry = value < 0

    result = value & 255
    parity = bin(result).count("1") % 2 == 0
    psw = (result & 128) | (64 if not result else 0)
    psw |= (16 if auxiliary else 0) | (4 if parity else 0) | 2 | carry
    return (a if op == "cmp" else result), psw


def cases(op):
    if op == "daa":
        return [(a, 0, flags) for a in range(256) for flags in (2, 3, 18, 19)]
    if op in ("inr", "dcr"):
        return [(a, 0, flags) for a in range(256) for flags in (2, 3)]
    return [(a, b, flags) for a in EDGES for b in EDGES for flags in (2, 3)]


def fixture(op, form, inputs):
    """Capture PSW and the result before any comparisons alter flags."""
    lines = ["        org 100h"]
    for index, (a, b, flags) in enumerate(inputs):
        lines += [f"        lxi h, {a * 256 + flags}",
                  "        push h", "        pop psw"]
        if op in ("inr", "dcr") and form != "a":
            lines += ["        lxi h, cell", "        mov m, a",
                      "        mov b, a"]
        elif form == "m":
            lines += ["        lxi h, cell", f"        mvi m, {b}"]
        elif form == "b":
            lines += [f"        mvi b, {b}"]

        if form == "immediate":
            mnemonic = {"adc": "aci", "sbb": "sbi", "cmp": "cpi", "sub": "sui"}[op]
            lines += [f"        {mnemonic} {b}"]
        else:
            lines += [f"        {op} {form}".rstrip()]
        if op in ("inr", "dcr") and form != "a":
            lines += [f"        mov a, {form}"]

        value, psw = reference(op, a, b, flags)
        lines += [
            "        push psw", "        pop d",
            f"        lxi b, {index}", f"        cpi {value}",
            "        jnz failure", "        mov a, e", f"        cpi {psw}",
            "        jnz failure",
        ]

    # Report a failing index and actual PSW in hex over the native console.
    lines += """        lxi d, success
        jmp report
failure:
        push d
        push b
        mov a, b
        call hexbyte
        pop b
        mov a, c
        call hexbyte
        pop d
        mov a, d
        push d
        call hexbyte
        pop d
        mov a, e
        call hexbyte
        jmp 0
report: mvi c, 9
        call 5
        jmp 0
hexbyte: push psw
        rrc
        rrc
        rrc
        rrc
        call digit
        pop psw
digit:  ani 15
        cpi 10
        jc decimal
        adi 7
decimal: adi 48
        mov e, a
        mvi c, 2
        call 5
        ret
cell:   ds 1
success: db 'OK$'
        end""".splitlines()
    return "\n".join(lines) + "\n"


def test(root):
    forms = {"adc": ("b", "m", "immediate"), "sbb": ("b", "m", "immediate"),
             "sub": ("b", "m", "immediate"), "cmp": ("b", "m", "immediate"),
             "inr": ("a", "b", "m"), "dcr": ("a", "b", "m"), "daa": ("",)}
    targets = (os.environ["XLT_TARGET"],) if "XLT_TARGET" in os.environ else ("msdos", "cpm86")
    flavors = (os.environ["XLT_EMIT"],) if "XLT_EMIT" in os.environ else ("nasm", "asm86", "masm3", "drasm86")
    assert all(t in ("msdos", "cpm86") for t in targets)
    assert all(f in ("nasm", "asm86", "masm3", "drasm86") for f in flavors)
    total = 0
    for flavor, target in product(flavors, targets):
        _, build, execute = runner(root, target)
        measured = 0
        for op, operands in forms.items():
            inputs = cases(op)
            for form in operands:
                for start in range(0, len(inputs), BATCH):
                    batch = inputs[start:start + BATCH]
                    try:
                        build("arith", fixture(op, form, batch), flavor)
                        result = execute("arith").strip().decode("ascii")
                    except Exception as error:
                        raise RuntimeError(f"{flavor}/{target} {op} {form} batch {start}") from error
                    if result != "OK":
                        if len(result) != 8:
                            raise SystemExit(f"{flavor}/{target} {op} {form}: unexpected output {result!r}")
                        index = int(result[:4], 16)
                        a, b, flags = batch[index]
                        expected_a, expected_psw = reference(op, a, b, flags)
                        raise SystemExit(
                            f"{flavor}/{target} {op} {form} case {start + index}, A={a:02X}, B={b:02X}, "
                            f"PSW={flags:02X}: expected {expected_a:02X}/{expected_psw:02X}, "
                            f"got {result[4:6]}/{result[6:8]}"
                        )
                measured += len(inputs)
                print(f"{flavor}/{target} {op} {form or '(implicit)'}: {len(inputs)}/{len(inputs)}", flush=True)

        total += measured
        print(f"PASS {flavor}/{target}: {measured} result/PSW cases", flush=True)
    print(f"PASS matrix: {total} cases across {len(flavors)*len(targets)} combinations", flush=True)


if __name__ == "__main__":
    with tempfile.TemporaryDirectory() as folder:
        test(Path(folder))
