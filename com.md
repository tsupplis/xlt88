# 8080 COM discovery and reviewed translation

CP/M 8080 binary at 0100h, not DOS/8086 COM. Start without a map:

```sh
./xlt88 --dialect=com --emit=asm80 input.com recovered.asm
```

COM discovery builds the shared source representation. The normal ASM80 emitter
writes `recovered.asm`; analysis also writes `recovered.map`. There is no extra
assembly output or special `.80` file. Exit **2** means the output is a draft
requiring review. Any selected native emitter also uses the normal pipeline;
its output remains a draft with TODOs, not a verified executable port.
Existing maps are preserved: use a reviewed map or another output basename.
If native lowering fails, the draft map is retained and exit is 1; select
ASM80 output to inspect recovered source. Output and state use normal staging.

Discovery follows direct jumps, both conditional paths and call targets from
0100h. Calls also seed their fall-through; returns, indirect jumps, restarts
and halt stop a path. Unknown bytes remain `unknown` in the map and appear as
DB in the listing solely to preserve bytes. Invalid opcodes, overlapping paths,
PCHL, XTHL and possible LXI pointers receive local `; XLT88 TODO:` comments.
No pointer or code/data certainty is inferred from a plausible numeric value.

Review the generated listing and map: correct candidate code ranges, replace
`unknown` with `code`, `data` or `word`, and add `ptr` at LXI instructions that
load addresses. Unmarked LXI operands remain numeric constants. A supplied map
is your reviewed classification; the translator checks its structural validity,
not its semantic truth. Unknown ranges are rejected. Then translate:

```sh
./xlt88 --dialect=com --com-map=recovered.map --emit=asm80 input.com reviewed.asm
./xlt88 --dialect=com --com-map=recovered.map --target=cpm86 input.com output.a86
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
arithmetic require manual review. Discovery does not resolve those constructs.
All 244 documented opcodes decode; small-program execution passes all eight
emitter/OS combinations, normalized equivalence and ASan/UBSan. LADCONF discovery currently finds only 134 candidate code bytes out of 22784;
inline-return-data handling blocks useful coverage. It is not a translated program. Reader returns ordinary source records; no second engine.
