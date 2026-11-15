# 8080 COM discovery and reviewed translation

CP/M 8080 binary at 0100h, not DOS/8086 COM. Start without a map:

```sh
./xlt88 --dialect=com --emit=asm80 input.com recovered.asm
```

COM discovery builds the shared source representation. The normal ASM80 emitter
writes `recovered.asm`; analysis also writes `recovered.map`. There is no extra
assembly output or special `.80` file. Exit **2** means the output is a draft
requiring review. Native emission is blocked without a reviewed map: TODOs do
not make an incomplete decode a native translation. Recovery is experimental.
Existing maps are preserved: use a reviewed map or another output basename.
If native lowering fails, the draft map is retained and exit is 1; select
ASM80 output to inspect recovered source. Output and state use normal staging.

Discovery follows direct transfers and call continuations from 0100h. Bounded
instruction analysis tracks return-address adjustments for inline arguments;
literal/register/stack hints can recover stack-loaded entries and inline jump
targets. An adjacent LHLD/PCHL also identifies an initial indirect pointer word
and its candidate destination; runtime writes may add or replace destinations.
No application names or fixed helper addresses are recognized.

These are candidate classifications. Nested-call and indirect-entry hints
assume balanced calls and no stack aliasing; they are marked for review.
Unknown reads, indirect control flow and self-modification remain limitations.
Byte runs are grouped without crossing referenced addresses or diagnostic
boundaries. Unknown regions remain explicit; DB preserves their bytes without
claiming they are data. Runs of at least four printable ASCII bytes are shown
as quoted text within each bounded DB run; quotes, controls and high bytes stay
numeric. Printable runs can extend to 64 bytes without crossing a label or
classification boundary. This improves readability without classifying printable bytes as data.
Native emission remains blocked for unreviewed recovery.

Review the generated listing and map: correct candidate code ranges, replace
`unknown` with `code`, `data` or `word`, and add `ptr` at LXI instructions that
load addresses. Unmarked LXI operands remain numeric constants. A supplied map
is your reviewed classification; the translator checks its structural validity,
not its semantic truth. Unknown ranges are rejected. Discovery adds `ptr` candidates when register
provenance reaches a memory access, stack/indirect transfer or known BDOS pointer
argument. Copies, pair increments, saved pairs and direct callees are followed;
byte arithmetic and overwrites invalidate provenance. Searches are bounded,
and candidates still require review. In-image integers used only for arithmetic
remain literal; the analyzer does not reinterpret every in-image number.

Use `gap` only for unused zero padding, not live storage. Nonzero gaps and known
references into them reject; native expansion must fit before the fixed origin (checked by the assembler).
BSS addresses beyond the file remain numeric. Preserve their layout explicitly
when needed; the reader cannot reconstruct missing ORG/DS declarations by itself.
Then translate:

```sh
./xlt88 --dialect=com --com-map=recovered.map --emit=asm80 input.com reviewed.asm
./xlt88 --dialect=com --com-map=recovered.map --target=cpm86 input.com output.a86
```

Map commands are lowercase; addresses are bare hexadecimal. Starts inclusive,
ends exclusive. Every file byte must be covered once; blank lines/# comments work.

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
| gap start end | Reviewed zero padding; emit ORG end to preserve the following fixed address |

Direct branches/memory operands already mean addresses. In-image references
must reach decoded boundaries; lxxxx labels enter normal [state](labels.md).
One-past-end pointers are supported. Other LXI constants stay numeric. Numeric
out-of-image addresses retain their value without promising target equivalence.

Limits: 1..65280 bytes, initial byte code, map lines ≤255 characters. Rejects
unclassified/overlapping ranges, truncated/cross-boundary instructions, malformed
pointer annotations and twelve undocumented aliases. Target expansion may fail.
Computed pointers, source-byte inspection, self-modifying code and layout-based
arithmetic require manual review. Discovery does not resolve those constructs.
All 244 documented opcodes decode. Small reviewed programs execute across all
eight emitter/OS combinations. Tests include relocated inline-argument helpers,
inline dispatch, stack-loaded entries, SP resets and large unresolved regions.
## VTL-2 end-to-end acceptance

`make vtlbench` assembles the unchanged source with real CP/M ASM80, recovers
ASM80 from its COM and checks byte-identical reassembly. The generated map is
reviewed for three non-code ranges and the 0800h fixed variable layout. Pointer
sites come from analysis, not a list embedded in the test or translator.
COM-input and source-input native translations produce identical transcripts
for arithmetic, stored programs, multi-record script input and return to console,
on CP/M-86 and DOS with all four emitters. This is reviewed-map acceptance;
automatic native COM translation remains open.
