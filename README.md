# xlt88

xlt88 translates Intel 8080 assembly source to NASM assembly for the 8086
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
transfer. A supported BDOS call uses `MVI C, constant` immediately
before an unlabelled transfer to address 5. Comments and blank lines
can appear between them. Addresses and service numbers can use simple
forward EQU aliases. A labelled transfer can have another caller,
so it requires a source edit before automatic translation.

| Source operation | DOS 1.0 | CP/M-86 1.1 |
| --- | --- | --- |
| BDOS 1, 2, 9 | Set AH to the service number and use INT 21h | INT E0h with CL already set, saving and restoring ES |
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
at the call/iteration setup and supplies decimal text for its low 16 bits.
An angle-grouped or escaped percent sign remains literal. `.RADIX` is not yet
supported, so decimal is the only output radix for this operator.

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

Nested macro definitions, pass conditionals (`IF1`/`IF2`), `NUL`, relocatable
modules and Z80 mode remain unsupported. This is a flat
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
- `norm8080.c`: optional 8080 source serialization (`--emit=8080`).
- `osrecog.c`: conservative tracking of the BDOS function argument.
- `symbols.c` and `state.c`: shared symbols and editable state.
- `expr.c`: owned expression trees and checked evaluation.
- `layout.c`: original 8080 layout and EQU dependencies.
- `trans.c`: CPU lowering with the injected `backend_t`.
- `msdos.c` and `cpm86.c`: native OS sequences.
- `target.c`: an owned `target_program_t`, independent of source lifetime.
- `nasm.c`: assembler formatting through an injected `emitter_t`.

`lower_8080` borrows the source and returns a separately owned target program.
The caller can lower the same source for both OS targets, destroy the source,
and replay either target to different emitters. Call `source_destroy` and
`target_destroy` for their respective programs, including on failure.

See [design.md](design.md) for the full architecture and remaining migration
work. Editable symbol-state files, typed expression trees, and conservative
source layout are available. Source instructions still use textual operand fields.
Intel ASM and a limited M80 8080 reader are implemented. MAC/RMAC and COM
readers are planned. NASM is implemented;
MASM 3, ASM86 and DRASM86 emitters remain planned.

Run `make test` with NASM, emu2, bin2cmd, and Python 3 on PATH. The script accepts
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
