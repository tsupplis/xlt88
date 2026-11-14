# xlt88

xlt88 translates Intel 8080 assembly source to NASM, ASM86, MASM 3 or RASM-86 assembly for the 8086
or 8088. Its OS targets are IBM PC DOS 1.0 and CP/M-86 1.1.

See [implementation progress](progress.md) for milestones, current measurements,
and acceptance criteria.

## Build and run

```sh
make
./xlt88 --target=msdos --dialect=intel source.asm dos.asm
nasm -f bin dos.asm -o program.com

./xlt88 --target=cpm86 --dialect=intel source.asm cpm.asm
nasm -f bin cpm.asm -o program.bin
bin2cmd program.bin program.cmd
```

The two-argument invocation defaults to MS-DOS and the Intel dialect.
For CP/M-86, use the default base-page insertion in the cpm86-cmdtools
`bin2cmd` utility to package a single-code-group, 8080-model CMD.
Both listings use origin 100h and start with the translated source.

## Editable symbol names

```sh
./xlt88 --state-out=names.sym source.asm dos.asm
# Edit only preferred_name fields in names.sym (JSON content).
./xlt88 --state-in=names.sym --state-out=names.sym source.asm dos.asm
./xlt88 --state-in=names.sym --emit=8080 source.asm named.asm
./xlt88 --state-in=names.sym --target=cpm86 source.asm cpm.asm
```

`symbols.c` builds one shared source symbol table; `state.c` reads and writes
version 2 state. The same table supplies definitions and expression names to
both emitters. EQU expressions remain authoritative in the source. IDs are
stable for an unchanged parsed source and scoped to its fingerprint. Changed
source requires a fresh state export; automatic merging is not yet supported.

State retains original names, preferred names, definition lines, EQU text and
resolved EQU values. Source layout records known original 8080 addresses;
generated 8086 addresses remain `null`. Version 1 state is accepted and upgraded
on export. Keep all fields except
`preferred_name` unchanged, including symbol array order. New preferred names
use letters, digits and underscores, start with a letter or underscore, and
contain at least two characters. Register names and expression operators are
reserved. Names are checked for duplicates without regard to case.

Each output is staged and replaced after successful generation. Validation
failures leave existing output files intact. Assembly and state are separate
file replacements, not a two-file transaction: a filesystem failure during the
second replacement can leave assembly updated with the previous state intact.
Source, assembly and state paths must be distinct; state input and output may
be the same file. Temporary filenames also fit 8.3.

## OS calls

The backend emits the native instruction sequence at each recognized
transfer. A supported BDOS call has a statically proven C at an unlabelled
transfer to address 5. `MVI C, constant` and `LXI B, constant` establish C;
known instructions that preserve C may intervene. C/BC writes, calls, control
transfers, labels and unrecognized operations invalidate the proof. Addresses and service numbers can use simple
forward EQU aliases. A labelled transfer can have another caller,
so it requires a source edit before automatic translation.

| Source operation | DOS 1.0 | CP/M-86 1.1 |
| --- | --- | --- |
| BDOS 1, 2, 9 | Set AH to the service number and use INT 21h | INT E0h with CL already set, saving and restoring ES |
| BDOS 10 | Pending: different buffer capacity/terminator contract | Native buffered input, preserving ES |
| BDOS 11 | Native status, byte result in A and L | Native status, preserving ES |
| BDOS 12 | Manual port | Return the native BDOS version |
| BDOS 25, 26 | Get current drive and set DTA | Get current drive and set DMA offset |
| BDOS 15, 16, 20–22 | Implemented for statically proven opaque FCB uses | Native open/close/read/write/create |
| BDOS 19, 23 | Pending | Native delete/rename |
| BDOS 17–18, 33–36 | Manual port | Native directory search and random-record operations |
| BDOS 40 | Manual port | Native zero-fill call; execution not yet verified |
| Transfer to 0, RST 0, or BDOS 0 | INT 20h | INT E0h with CL and DL set to zero |

Other BDOS services, unproven DOS FCB uses,
and ambiguous service numbers
produce source-line errors for manual porting. Review register and flag
dependencies against the native OS interface. Use an explicit warm boot
for portable termination. RET remains a near return. On CP/M-86,
`LXI SP` and `SPHL` select the program's data segment for SS before
setting SP. Programs that choose a private stack must reserve its
storage in the source.

Direct CALL and JMP targets, including conditional forms, accept
simple labels, numeric literals, and EQU chains ending in either.
Compound target expressions such as `0+5` require a source edit.
A numeric transfer that matches a unique source label binds to that label in
the generated program. Multiple labels at the same address require a named
transfer. Unmatched numeric targets retain their literal value and require
manual review; they are not automatically relocated. Address constants in
ordinary immediates are also not automatically treated as pointers.

Expression trees retain symbol IDs and grouping for arithmetic, bitwise
operators, strings, lists, and current location (`$`). Known constant overflow,
invalid arithmetic, undefined expression symbols and cyclic EQU dependencies
produce errors. Source layout handles instruction sizes, DB strings, numeric
DW entries, DS, ORG and ALIGN. Unsupported sizes, including DW strings, leave
subsequent addresses unknown. Generated addresses still require an assembler
map. EQU values in state describe the source layout, while target EQU
expressions remain symbolic and are evaluated by the target assembler.


## Source dialect and data layout

The Intel dialect accepts column-one labels and indented instructions.
An initial `ORG 100h` is optional. Use a standalone `ALIGN 2` before a
word table or buffer to align its label. Alignment inserts zero padding
and accepts powers of two up to 256. Ordinary data declarations keep
their original layout.

Unknown mnemonics and nonzero RST vectors produce source-line diagnostics.
Preprocess macros and compound lines using `!` before translating.
Operands can use Intel's `AND` expression operator, for example
`(TABLE AND 0FFH)`. Expression references to source labels use their
offsets in the flat image. The emitter escapes source labels so that
names such as `CPU` can coexist with NASM keywords.

### M80 reader subset

```sh
./xlt88 --dialect=m80 --target=cpm86 source.asm target.asm
./xlt88 --dialect=m80 --emit=8080 source.asm review.asm
```

The M80 reader accepts the existing 8080 instruction/data syntax, known
operations at column one, explicit labels with indentation, and `.8080`.
`TITLE`, `SUBTTL`, `PAGE`, `.LIST` and `.XLIST` are retained as comments.
`IF`/`IFT`, `IFE`/`IFF`, `IFDEF`/`IFNDEF`, `ELSE` and `ENDIF` support
nested conditional assembly. Numeric conditions require absolute values known
at that point; definedness tests inspect previously encountered active symbols.
`INCLUDE` and `.INCLUDE` expand exact bare or quoted filenames, resolving relative
paths against the containing file. Inactive branches do not open includes.
Diagnostics retain the original filename and physical line after expansion.

Conditional blocks must balance within each file. Include cycles are rejected;
nesting is limited to 32 source files and 64 conditions per file. No implicit
filename extension or include search path is supplied.

`name MACRO parameters` through `ENDM` supports up to 32 positional parameters,
comma-separated actual arguments, angle-grouped argument lists, omitted arguments
as empty text, and nested calls (maximum depth 32). Definitions must precede use.
`LOCAL` declarations at the start of a body provide up to 32 unique labels per
call. The `__m80_` symbol prefix is reserved for these generated names. Ordinary
tokens are substituted case-insensitively; quoted strings and comments retain
literal text unless a quoted parameter is explicitly prefixed by `&`. Double-semicolon comments are omitted from expansion. Macro-body
conditions and includes are processed after substitution, with includes relative
to the definition file. Conditional blocks must balance within each expansion.
Normalized output can be read back with the default Intel reader.

`REPT count` repeats a block using a previously known absolute count in
0..65535. `IRP name,<items>` substitutes each item (up to 32); `IRPC name,text`
substitutes each character, with optional enclosing angle brackets. Empty IRP
and IRPC inputs expand once with empty substitution. Blocks end at matching
`ENDM` and may nest, including inside macros. `EXITM` stops the innermost
macro/repetition, discarding its remaining iterations; outer expansion resumes.
An inactive `EXITM` has no effect. The shared nesting limit is 32, with a total
budget of 65536 macro calls/repetition iterations and 65536 output records.

`LOCAL` remains restricted to the start of named macro definitions. Nested
repetition parameters must have distinct names from enclosing parameters and
locals; shadowing is diagnosed.

Macro `&` separates adjacent substitution tokens, including through nested
expansion; final statement names are joined before entering the shared IR.
Within strings, `&parameter` inserts its actual text. Argument `!` protects the
next character, including commas, semicolons and spaces. Leading `%expression`
in a macro argument or IRP item evaluates a previously known absolute value
at the call/iteration setup and supplies text in the active radix for its low
16 bits. A leading zero is supplied when the first digit is alphabetic.
An angle-grouped or escaped percent sign remains literal. Generated results
ending in B/D that conflict with the reader's explicit-suffix grammar are
rejected; use a literal argument in those cases.

`IFB`/`IFNB` test an angle-delimited argument for zero/nonzero text length.
`IFIDN`/`IFDIF` compare two angle-delimited arguments exactly, including case,
quotes and interior whitespace. Escapes and nested angle groups are accepted;
unbracketed or extra arguments are diagnosed.

`name SET expression` assigns a mutable 16-bit absolute value. Its expression
must be resolvable from earlier definitions; subsequent assignments may refer
to the previous value. Each emitted expression snapshots current SET values,
including inside repetitions and includes. Register operands and quoted strings
retain their original meaning. SET cannot redefine an EQU/address label or
later become one. Mutable names are preprocessing state, not editable label
entries in the `.sym` file.

Expressions now accept `MOD`, `SHL`, `SHR`, `OR`, `XOR`, `NOT`, `HIGH`, `LOW`
and `EQ`/`NE`/`LT`/`LE`/`GT`/`GE`. Word-form shifts bind with multiplication:
`1234h SHR 8 + 128` is 146. Existing `<<`/`>>` spellings retain their earlier
precedence. Comparisons return 65535 or zero; NOT complements the low 16 bits.
General arithmetic retains the shared checked host-integer contract rather
than emulating every M80 16-bit overflow rule.

`.RADIX expression` sets the source base from 2 through 16 (default 10).
The directive itself always evaluates its expression in decimal. Active changes
persist through includes and macro expansion. Unsuffixed numeric expression
operands are converted to explicit decimal literals before layout/lowering;
registers, strings and unexpanded macro text are preserved. B/D/H/O/Q suffixes
and the existing 0x extension override the active base. Historical M80 versions
have ambiguous B/D handling at high radices; this reader consistently gives
explicit suffixes priority rather than reproducing those version differences.

`NUL` tests the remaining expression text: no text after whitespace yields
65535; any remaining text yields zero. It does not resolve that text as a
symbol or expression. It consumes the remainder, so use IFB/IFNB when testing
an angle-delimited macro argument.

Nested macro definitions, pass conditionals (`IF1`/`IF2`), relocatable modules
and Z80 mode remain unsupported. This is a flat
8080 source subset, not complete MACRO-80 compatibility; expression grammar
and symbol identity still follow xlt88's current contracts.

Both readers handle LF, CRLF, bare CR, final lines without a newline and the
CP/M Ctrl-Z end marker. Physical source line numbers are preserved. Embedded
NUL bytes and lines exceeding 254 characters are diagnosed. Intel's column-one
label convention remains in effect unless a colon explicitly marks a label.

## Code and tests

The driver in `xlt88.c` selects dependencies and owns streams. The pipeline is:

- `dialect.c`: Intel/M80 lexical parsing into an owned `source_program_t`.
- `m80.c`: M80 preprocessing order, conditional assembly and includes.
- `macro.c`: owned macro definitions, argument substitution and local labels.
- `mexpr.c`: M80 radix literals, NUL normalization and percent formatting.
- `norm8080.c`: optional 8080 source serialization (`--emit=8080`).
- `osrecog.c`: conservative tracking of the BDOS function argument.
- `symbols.c` and `state.c`: shared symbols and editable state.
- `expr.c`: owned expression trees and checked evaluation.
- `layout.c`: original 8080 layout and EQU dependencies.
- `trans.c`: CPU lowering with the injected `backend_t`.
- `msdos.c` and `cpm86.c`: native OS sequences.
- `target.c`: an owned `target_program_t`, independent of source lifetime.
- `nasm.c`, `asm86.c`, `masm.c`, `drasm86.c` (with shared `asmfmt.c`): assembler formatting through an injected `emitter_t`.

`lower_8080` borrows the source and returns a separately owned target program.
The caller can lower the same source for both OS targets, destroy the source,
and replay either target to different emitters. Call `source_destroy` and
`target_destroy` for their respective programs, including on failure.

See [design.md](design.md) for the full architecture and remaining migration
work. Editable symbol-state files, typed expression trees, and conservative
source layout are available. Source instructions still use textual operand fields.
Intel ASM and limited M80/MAC readers are implemented. RMAC single-module
source placement is available through `--dialect=rmac`; see [rmac.md](rmac.md).
COM input remains planned. NASM, ASM86, MASM 3 and DRASM86 (RASM-86) are implemented.

Run `make test` with NASM, emu2, bin2cmd, cpm86_asm86, cpm86_gencmd,
pcdev_masm3, pcdev_rasm86, pcdev_link2, pcdev_exe2bin, and Python 3 on PATH. The script accepts
`EMU2` and `BIN2CMD` overrides. It checks both Hello World binaries,
the generated native calls, diagnostics, exact packed and aligned data,
the 8080 auxiliary carry produced by ANA and ANI through PSW and DAA,
and the Microcosm diagnostic translated from `examples/tst8080.asm`
for each target.
It also runs both native FCB copy examples across a 128-record boundary.
The arithmetic suite compares translated execution against an 8080
reference model and reports counts by instruction and operand form.

`make test` checks that the assembled fixtures collectively cover every
8080 mnemonic in the translator table (78 of 78 today). The hardware
instruction fixture checks the emitted bytes for `DI`, `EI`, `HLT`,
`IN`, and `OUT`. It runs `NOP` followed by `RST 0` on both targets
and separately checks rejection of `RST 1` through `RST 7`. This is
mnemonic coverage. Operand forms, flag combinations, and hardware side
effects require separate tests.

## Native FCB copy examples

```sh
nasm -f bin examples/copy-dos.asm -o xcopy.com
nasm -f bin examples/copycpm.asm -o xcopy.bin
bin2cmd xcopy.bin xcopy.cmd
```

Run `XCOPY INPUT.DAT OUTPUT.DAT` with two exact 8.3 filenames and a
fresh destination. A failed copy may leave a partial output file.
The programs copy 128-byte records. DOS writes a zero-padded record
when the final read reports a partial record.

Both examples copy the overlapping command-line FCB names into private
buffers. The DOS port sets the DTA and handles DOS status 3. The CP/M-86
port sets the DMA address and preserves ES around each returning BDOS
call. Automatic CP/M-86 sequential FCB translation is now tested separately by
`tests/fileops.py`, including empty files and copies through 260 records.
General CP/M-to-DOS FCB layout adaptation remains unfinished; the opaque
sequential subset below is implemented.

See [native API coverage](api.md) for the BDOS 2.2 call inventory and
DOS 1.00 porting requirements.

## DOS FCB translation contract

Input FCBs are CP/M FCBs for every target. Selecting `--target=msdos` must
translate the source layout and its field accesses; callers never declare
DOS FCBs. Functions 15, 16, 20, 21 and 22 now lower when FCB analysis proves that
metadata is opaque to the source. Dedicated zero-initialized DS reservations
of at least 33 bytes become native DOS FCB storage in place; 33–35-byte
reservations grow to 36. All following symbolic references relocate normally.
The bounded 12-byte name-copy loop from default FCBs is recognized by its
instructions, not its name. No source option, shadow FCB or runtime is used.

This initial analysis is deliberately restrictive: it rejects metadata accesses,
unrecognized calls, unproved memory writes, pointer/stack escapes, alternate
entries to the copier, unproved DMA buffers and unrelocatable addresses.
It currently recognizes the contiguous eight-instruction name-copy idiom in
tests/copy.asm. This is a supported program pattern, not general FCB analysis.

DOS partial final reads become successful padded record reads. Create probes
for an existing file and reports failure without truncation. Disk-full and
hardware error paths remain untested. The unchanged CP/M copy fixture, smaller
FCB storage, conditional calls, byte comparisons, EOF and extent boundaries
are exercised by tests/dosfcb.py. The handwritten DOS copy example is a reference, not evidence of
automatic CP/M FCB translation.

CP/M-86 search and random I/O run in tests/randio.py. BDOS 40 is emission-tested
only because the installed emulator lacks that service; use CPM_ZFILL=1 with
a supporting emulator to run its zero-fill execution test.

## ASM86 output

All assembly emitters except NASM write CRLF line endings and a final Ctrl-Z
(0x1A), including normalized 8080 output. NASM output uses LF without Ctrl-Z.

`--emit=asm86` selects DRI assembler syntax from the same owned target program:

```sh
./xlt88 --target=cpm86 --emit=asm86 examples/hello.asm hello.a86
cpm86_asm86 hello.a86
cpm86_gencmd hello 8080
emu2 hello.cmd
```

Validation uses ASM86 1.1N2 and GENCMD's combined 8080 memory model,
with origin 100h. The emitter writes CSEG, separate labels, RB reserves,
and dot-address memory operands. GENCMD supplies the reserved storage in
this tested workflow. Conditional jumps expand to an inverse short jump
and an unconditional jump, so output sizes can differ from NASM.
Source names are annotated in comments; assembler names are normalized and
collision-checked because ASM86 ignores underscores. Forward equates are
ordered after their dependencies, with location anchors preserving `$`.
Constant expressions outside -32768 through 65535 and numeric branch targets
without a symbolic destination are rejected without replacing the output.
Unresolved expressions still follow the assembler's 16-bit arithmetic;
full equivalence for arbitrary symbolic overflow is not established.

`tests/emitters.py` rejects assembler errors and ignored-syntax warnings, then
compares executed CPU, console, memory, alignment and CP/M FCB copy fixtures
with NASM output. It accepts `XLT88`, `ASM86`, `GENCMD`, and `EMU2` overrides.
ASM86, MASM 3 and RASM-86 execution validation cover CP/M-86 and DOS 1.0.
For ASM86 DOS validation, the test converts the GENCMD combined image into
a COM file after checking its single-group header and discarding the
128-byte CMD header and 100h base-page prefix. It retains zero-filled
reserved storage; running the DOS-lowered CMD directly would use the wrong OS.

## MASM 3 output

```sh
./xlt88 --target=msdos --emit=masm3 examples/hello.asm hello.asm
pcdev_masm3 'hello.asm,hello.obj;'
pcdev_link2 'hello.obj,hello.exe;'
pcdev_exe2bin hello.exe hello.com
emu2 hello.com
```

The tested toolchain is Microsoft MACRO Assembler 3.00 and LINK 2.40.
Build tools run under DOS 3.3 in the test harness; generated DOS programs
run with `EMU2_DOSVER=1.0`. The linker's `no STACK segment` warning is
expected for this COM layout; tests permit only that linker warning and
require zero assembler warnings/errors. LINK may return status 1 for it.
For `--target=cpm86`, package the resulting flat COM with
`bin2cmd hello.com hello.cmd` and execute the CMD.

MASM output uses CRLF lines, a single segment at origin 100h, explicit
near jumps and typed direct-memory operands. Reserves use zero-filled DUP;
conditional assembly suppresses zero-count DUP, including already aligned
locations. The shared legacy formatting module owns aliases, deferred
equates and branch expansion. It never modifies the target program.

`make test` runs the emitter comparison matrix for ASM86/MASM 3/RASM-86 and both OS
targets. `XLT_EMIT=masm3 XLT_TARGET=msdos python3 tests/emitters.py` selects
one combination. Additional tool overrides are `MASM3`, `LINK2`, `EXE2BIN`.
The checks cover the full CPU diagnostic, opcode assembly, flags, forward
symbols, name collisions, direct memory, zero reserves, alignment and copies
of 0, 1, 129 and 260 records. This is fixture coverage, not proof of every
assembler expression or full OS service coverage.

## DRASM86 output (RASM-86)

The planned DRASM86 flavor is implemented for Digital Research's **RASM-86
1.4a**, the installed relocatable assembler. Use `--emit=drasm86`:

```sh
./xlt88 --target=msdos --emit=drasm86 examples/hello.asm hello.a86
pcdev_rasm86 hello
pcdev_link2 'hello.obj,hello.exe;'
pcdev_exe2bin hello.exe hello.com
emu2 hello.com
```

RASM-86 produces an OBJ, whereas ASM86 produces an H86. The tested packaging
path uses Microsoft LINK 2.40 and EXE2BIN, preserving the flat origin-100h
image. For CP/M-86, select `--target=cpm86`, build the same way, then run
`bin2cmd hello.com hello.cmd` and `emu2 hello.cmd`. The default Digital
Research LINK-86 EXE layout is not the validated packaging path.

The renderer emits CRLF, an entry label and `END` entry reference. Address
arithmetic uses label differences within the segment; explicit location
labels preserve `$` in instructions, data, alignment and deferred equates.
Aliases, direct-memory formatting and branch expansion share `asmfmt.c`.
This adds assembler syntax support without changing CPU or OS lowering.

`XLT_EMIT=drasm86 XLT_TARGET=msdos python3 tests/emitters.py` validates one
combination; omit `XLT_TARGET` for CP/M-86. `RASM86` overrides the assembler
command. Assembly must report zero errors with no ignored-syntax diagnostics;
the LINK missing-STACK warning remains expected for the COM layout.
All four output flavors are implemented and run the arithmetic reference
suite on both OS targets. The legacy-emitter matrix covers the CPU diagnostic,
flags, opcode assembly, memory/location expressions, alignment and FCB copies.

## Arithmetic emitter matrix

`make test` runs `tests/arith.py` across NASM, ASM86, MASM 3 and DRASM86,
each targeting DOS 1.0 and CP/M-86. Every combination uses the same 8,800
cases and checks both the result and complete PSW against the Python 8080
reference model: 70,400 comparisons for a full matrix run.

The set covers ADC/SBB/SUB/CMP in register, memory and immediate forms,
INR/DCR in accumulator, register and memory forms, and DAA. Binary arithmetic
uses 14 boundary operands with both carry states; INR/DCR cover every byte
with both carry states; DAA covers every byte and all input AC/C states.
This is the existing arithmetic case set across every renderer, not exhaustive
8080 instruction or operand-pair coverage.

Batches contain at most 64 cases, keeping DAA within MASM 3's assembler
memory limit without reducing coverage. `tests/toolrun.py` shares toolchain
invocation, strict assembler checks, packaging and execution with the emitter
fixtures and removes stale build products before each batch. Failures identify
the emitter, target, operation, operand form and batch/case.

For a focused run:

```sh
XLT_EMIT=masm3 XLT_TARGET=msdos python3 tests/arith.py
```

Omit either variable to run all values of that dimension. The tool overrides
from the emitter fixtures also apply, including `XLT88`, `NASM` and `BIN2CMD`.

## Real-program acceptance baseline

Run `make corpus` to regenerate [corpus.md](corpus.md). The inventory preserves
source bytes (including historical encodings), excludes existing 8086 output
references, and distinguishes normalization from translation/rendering,
assembly and execution. It discovers additional `.asm` files automatically;
only explicitly approved smoke fixtures execute. A failed established smoke
fixture makes the command fail; known unsupported programs are reported as
blocked rather than counted as passing tests. The survey currently uses
Intel syntax except for FIG-Forth's M80 listing syntax.

M80 accepts column-one numeric conditional names (`IF`, `IFT`, `IFE`, `IFF`)
as labels when followed by a known operation and the remaining text cannot
form a conditional expression. Thus `IFF DW DOCOL` and `IFF RST 3` are labels,
while `IFF DW-1` and `IF DW EQ 1` remain conditions on the symbol `DW`.
Indentation retains directive interpretation, and a colon explicitly marks a
label. This resolves the FIG-Forth spelling without reserving opcode-named
symbols or modifying its source.

## Source addresses and fixed regions

Expressions such as `NEXT+3`, `routine+1` and `$+3` in direct transfers are
resolved against original 8080 instruction addresses. A unique instruction
boundary receives an internal target label; offsets into instructions or data,
unknown addresses and ambiguous overlaid code remain rejected. Source byte
offsets are never reused as offsets into the expanded 8086 instruction stream.
Named and numeric transfer behavior otherwise retains the existing contracts.

After the optional initial `ORG 100h`, known forward ORGs preserve the requested
physical offset by emitting a target origin event. Zero padding is calculated
at assembly time. Every renderer rejects translated code that outgrows the
region; ASM86/RASM-86 have explicit guards because negative RB counts can
otherwise be accepted silently. Known, non-overlapping fixed regions may appear out of address order; the
region planner sorts their emission while retaining source identities and
origins. Unknown sizes in reordered regions, overlaps and expansion overflow
remain rejected. No automatic relocation or segmented layout is inferred.

`tests/flow.py` executes boundary jumps/calls, fixed-region placement, BDOS
recognition and native console fixtures with all four emitters on both targets.
It checks CP/M buffered input at full capacity and empty input with a guard
byte, and console status with redirected input (A=L=0 in installed emu2).
Ready-status execution with a live console is not established by that test.


### Explicit console-only FIG-Forth port

Generate a separate source and translate it normally:

```sh
python3 tests/figforth.py /tmp/forth.80
./xlt88 --dialect=m80 --target=cpm86 --emit=asm86 /tmp/forth.80 /tmp/forth.a86
```

The original example is preserved. The port replaces BIOS console calls with
BDOS calls and rejects actual disk/printer requests with a diagnostic and exit.
It does not add a CP/M runtime. `tests/forth.py` checks startup, arithmetic,
a new word definition, execution, exit and disk rejection on both targets with
all four assemblers. `tests/console.py` validates native BDOS 6 and ready status,
using emu2 keyboard scripts and a terminal input handle in 8088 CPU mode.
Both run under `make test`.

Data-list initializers are emitted individually: `$` advances for each item,
as verified with MACRO-80. This preserves threaded Forth branch displacements
such as `DW ZBRAN,ULES1-$` across assembler flavors.


The next reference comparisons are recorded in [refs.md](refs.md), covering
VTL-2, WASH and DISK7. Native DOS delete/rename now use compatible name fields;
see [api.md](api.md) for execution coverage and the emulator collision limitation.


`make vtlbench` measures unchanged VTL-2 against its supplied direct xlt86
reference. It validates reference execution and records each xlt88 target as
executed or explicitly blocked. See [vtlbench.md](vtlbench.md). Native drive
selection (BDOS 14) and reset (BDOS 13) are available on both targets; reset
composes native operations to establish the source drive/DMA/result contract.


The owned target program now has a conservative branch-size pass (`shrink.c`).
It folds an inverse-condition/skip/jump sequence into a direct conditional jump
only when shared 8086 encoding bounds prove rel8 reach. Legacy emitters respect
that proof instead of expanding the condition again. Far branches and paths
across data, alignment, origins or unknown instructions retain their safe form.

### MAC reader subset

Use `--dialect=mac` to read Digital Research MAC source, including unchanged
`examples/disk7.mac`. The reader handles embedded dollar separators, optional
line numbers, statement separators, doubled quotes, macros, nested conditions,
labelled ORG/IF/END and versioned SET assignments. See [mac.md](mac.md) for
contracts and limits. Successful normalization does not imply complete BDOS
translation or executable DISK7 support.

### Bounded native service selection

BDOS recognition now follows register copies and direct control flow, including
inputs to shared routines. Up to four proven function values can produce native
conditional dispatch; unknown values remain errors. DOS FCB address loads may
be separated from calls by proven-safe register operations. Auxiliary/list
output (4/5) now assembles on both targets, with device execution still unverified.
See [api.md](api.md) for the distinction between lowering and execution coverage.

### VTL-2 acceptance

Unmodified `examples/vtl2.asm` now assembles and passes the xlt86-reference
transcript on CP/M-86 with all four emitters (`make vtlbench`). Its variables
remain at 0800h. General flag-liveness and short-branch improvements make it fit;
no source-specific patch or relocation is used. DOS now passes the same
interactive acceptance through all four emitters using the first native default
FCB. Arbitrary memory accesses have native target semantics.
Native reader input is now emitted on both targets; CP/M-86 login-vector output
is execution-tested. See [progress.md](progress.md) and [api.md](api.md) for limits.

Script-file acceptance now also compares unchanged VTL-2 with the unchanged
xlt86 reference: stored programs, multi-record input, empty/missing files and
Ctrl-Z termination pass on all four CP/M-86 emitters (`tests/vtlscr.py`).
DOS random read and set-random-record calls are available for audited opaque
FCBs. Native CP/M-86 attribute, user-area and drive-protection calls have been
added; execution limits are recorded in [api.md](api.md).

The script-file comparison now also passes on DOS with all four emitters.
The first default FCB uses native PSP storage; explicit incompatible metadata
accesses are diagnosed. Scripts that inspect arbitrary OS memory may require
porting. No new command-line option or compatibility runtime is introduced.

### WASH15 reference and additional file services

`tests/wash.py` translates unchanged WASH15 through all four CP/M-86 emitters
and compares five complete transcripts with the supplied patched reference.
Results and remaining DOS FCB blockers are recorded in [wash.md](wash.md).
Generic guarded memory-selector recognition enables its service dispatch;
unmatched CP/M-86 values use native pass-through with porting warnings; DOS retains native termination.
The original interrupt policy is preserved and still needs hardware review.
DOS random write/file size now execute across all four emitters for validated
FCB plans. CP/M-86 selected-drive reset is emitted, with state execution still
limited by emulator support. See [api.md](api.md) for field and error contracts.

For post-translation review, see [porting.md](porting.md): implicit page alignment,
fixed placement, segments, stacks, FCBs and the boundary between generic analysis
and explicit manual adjustments.


CP/M-86 now has native lowering for all 39 non-reserved CP/M 2.2 entry points.
Functions 27/31 return ES:BX and emit explicit pointer-consumer review warnings;
this does not complete automatic flat-memory translation or behavioral coverage.
See [api.md](api.md). WASH reference comparisons include CMD sizes; DISK7 has
repeatable reference-only scenarios and a blocker inventory in [diskref.md](diskref.md).

The current cross-project snapshot and proposed next batch are in [status.md](status.md).


CP/M-86 now accepts unknown BDOS selectors and native extensions through an
injected native-call hook. Known calls retain their existing adaptations;
unknown calls use native contracts (including functions 6/13 and segment
results) and emit port-review warnings. DOS remains strict. See api.md.
Seven additional WASH transcript/file comparisons are in [washfile.md](washfile.md).
The separate [washport.md](washport.md) checks an explicit native allocation
port across all emitters. Its smaller allocation limits are simulated with
base-page overrides and guard bytes; CMD sizing policy remains deferred.
WASH's DOS analysis is in [washdos.md](washdos.md). DISK7 is an assembly/port-review
checkpoint, not a newly validated executable.

The current functional additions include DOS buffered input for capacities
0..254 (native DOS editing/completion, explicit capacity-255 rejection) and
generic FCB/DMA declarations with separate label and DS records. See api.md
and tests/conbuf.py, tests/storage.py. RMAC coverage is a source-reader
milestone, not complete relocatable linking.
