"""Close operand/flag gaps left by the legacy diagnostic and arithmetic suite.

Each case observes the full PSW immediately, keeping flag optimisations honest.
The original arithmetic suite retains its exhaustive DAA/INR/DCR checks; this
suite checks every byte-register encoding and preservation by unary operations.
"""
from itertools import product
from pathlib import Path
import os
import tempfile

# Permit an explicitly selected CPU-validation emulator without changing the
# emulator used by the native console/filesystem suites (see status.md).
if "XLT_CPU_EMU" in os.environ:
    os.environ["EMU2"] = os.environ["XLT_CPU_EMU"]

from arith import BATCH, fixture
from toolrun import runner


def groups():
    edges = (0, 8, 15, 16, 127, 128, 255)
    for op in ("add", "adc", "sub", "sbb", "cmp", "ana", "ora", "xra"):
        for form in ("a", "b", "c", "d", "e", "h", "l", "m", "immediate"):
            # A is both source and destination in the accumulator form.
            inputs = [(a, a if form == "a" else b, flags)
                      for a in edges for b in (edges if form != "a" else (0,))
                      for flags in (2, 0xd7)]
            yield op, form, inputs
    for op in ("inr", "dcr"):
        for form in ("a", "b", "c", "d", "e", "h", "l", "m"):
            yield op, form, [(a, 0, flags) for a in edges for flags in (2, 0xd7)]
    for op in ("rlc", "rrc", "ral", "rar", "cma", "stc", "cmc"):
        # Exercise every accumulator and both extremes of preserved flags.
        yield op, "", [(a, 0, flags) for a in range(256) for flags in (2, 0xd7)]


def main():
    total = 0
    with tempfile.TemporaryDirectory() as directory:
        work = Path(directory)
        for target, flavor in product(("msdos", "cpm86"),
                                      ("nasm", "asm86", "masm3", "drasm86")):
            _, build, execute = runner(work, target)
            measured = 0
            for op, form, inputs in groups():
                for start in range(0, len(inputs), BATCH):
                    build("cpufinal", fixture(op, form, inputs[start:start+BATCH]), flavor)
                    output = execute("cpufinal").strip()
                    assert output == b"OK", (target, flavor, op, form, start, output)
                measured += len(inputs)
            total += measured
            print(f"{target}/{flavor}: {measured} operand/result/PSW checks passed", flush=True)
    print(f"CPU closure matrix: {total} checks passed", flush=True)


if __name__ == "__main__":
    main()
