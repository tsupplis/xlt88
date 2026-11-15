# Annotated 8080 COM input — initial implementation

CP/M 8080 binary at 0100h, not DOS/8086 COM. Classification is explicit.

```sh
./xlt88 --dialect=com --com-map=input.map --emit=8080 input.com review.asm
./xlt88 --dialect=com --com-map=input.map --target=cpm86 input.com output.asm
```

Map commands are lowercase; addresses are bare hexadecimal. Starts inclusive,
ends exclusive. Every byte must be covered once; blank lines/# comments work.

```text
code 0100 010b
data 010b 0112
ptr 0100
```

| Command | Meaning |
| --- | --- |
| code start end | Complete documented 8080 instructions |
| data start end | Literal bytes, including 1Ah |
| word start end | Little-endian pointer words; even range length |
| ptr address | LXI at this address loads a pointer, not an integer |

Direct branches/memory operands already mean addresses. In-image references
must reach decoded boundaries; lxxxx labels enter normal [state](labels.md).
One-past-end pointers are supported. Other LXI constants stay numeric. Numeric
out-of-image addresses retain their value without promising target equivalence.

Limits: 1..65280 bytes, initial byte code, map lines ≤255 characters. Rejects
unclassified/overlapping ranges, truncated/cross-boundary instructions, malformed
pointer annotations and twelve undocumented aliases. Target expansion may fail.
Computed pointers, source-byte inspection, self-modifying code and layout-based
arithmetic require manual review. No heuristic code/data recovery is promised.
All 244 documented opcodes decode; small-program execution passes all eight
emitter/OS combinations, normalized equivalence and ASan/UBSan. Real-program
COM acceptance is next. Reader returns ordinary source records; no second engine.
