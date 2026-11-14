# xlt88 implementation progress

This ledger tracks the agreed design and implementation scope. Status describes
working code, not merely a design proposal. Update it when a milestone gains
implementation and tests. Detailed OS service coverage lives in api.md.

## Current measurements

| Measure | Current result | What it establishes |
| --- | --- | --- |
| 8080 mnemonic fixtures | 78/78 | Every translator mnemonic appears in an assembled fixture; not exhaustive instruction behavior |
| Arithmetic execution | 8,800 cases per OS target | Tested arithmetic/flag combinations agree with the reference model |
| CP/M-86 native BDOS lowering entries | 21/39 non-reserved CP/M 2.2 functions | Functions 0, 1, 2, 9, 12, 15–23, 25, 26, 33–36, 40 have lowering code; function 40 execution unverified |
| DOS native BDOS lowering entries | 11/39, five with restricted FCB analysis | 0, 1, 2, 9, 25, 26 plus 15, 16, 20–22 for proven opaque FCB uses; not general file/API coverage |
| Input readers | Intel implemented; M80 partial; 2 pending | M80 flat 8080 syntax/listing subset implemented; MAC/RMAC and COM pending |
| Final assembler emitters | 4/4 | NASM, ASM86 1.1N2, MASM 3.00 and DRASM86 (RASM-86 1.4a); all three legacy flavors execute DOS 1.0 and CP/M-86 fixtures |
| Normalized 8080 output | Implemented | Separate output from the parsed source, including preferred symbol names |
| Symbol state | Version 2 implemented | Names, source-scoped IDs, EQU text/values and known original addresses; version 1 import supported |

The 39-function denominator is BDOS 0–37 and 40; 38 and 39 are reserved.
The DOS column measures translation of that source API. Supporting all native
DOS 1.0 operations is an additional target, not another name for this count.
Returning the native CP/M-86 version is an intentional target behavior, not
emulation of a CP/M-80 machine identity.

## Major milestones

| Step | Status | Implemented | Remaining / acceptance condition |
| --- | --- | --- | --- |
| 1. Separate pipeline modules | Done for current capabilities | Reader, CPU lowering, OS backends, owned target program, NASM renderer and driver; injected dependencies and lifecycle tests | Maintain separation as new passes are added |
| 2. Shared symbols and editable state | Partial | Stable IDs scoped to source fingerprint; preferred-name import/export; conflicts/stale-state checks; source layout and numeric branch binding to unique labels | Rebind after source edits; generated-label identities; explicit aliases/address annotations; generated addresses from assembler maps |
| 3. Typed intermediate representation | Partial | Owned expression trees, ID-bound symbol leaves, checked arithmetic, EQU dependencies; typed target event/operand kinds | Typed source instructions/operands; structured target memory operands; full source provenance; remove remaining textual interpretation between passes |
| 4. Correct 8080 instruction behavior | Partial | 78 mnemonic fixtures, CPU diagnostic, arithmetic cases, PSW/DAA checks, both targets | Explicit instruction/operand/flag coverage matrix; fill untested combinations and boundary cases; distinguish executed checks from emitted-byte checks and hardware-only behavior |
| 5. Complete BDOS 2.2 to CP/M-86 | Early | Twenty-one native lowering entries, including search/random operations | Implement and validate remaining 18 source functions or document required explicit ports; far-pointer and system-data consumers need representation-aware translation |
| 6. Complete BDOS 2.2 to DOS 1.0 | Early | Six existing entries plus five proven opaque-FCB lowerings | Implement missing source semantics through native DOS operations; FCB layouts, statuses, user areas, attributes, drive policy and disk structures require explicit decisions, not call-number substitution |
| 7. Native DOS 1.0 operation support | Pending | Target version and local reference sources identified | Inventory native services; define source representation for DOS-only operations; verify each entry against DOS 1.0 rather than assuming later-version behavior |
| 8. Additional source readers | Partial | Intel ASM plus M80 8080 subset with conditions/includes/macros/LOCAL/repetition/EXITM/text operators/string conditions | M80 remaining expression/word-arithmetic semantics/pass conditions/relocation; MAC/RMAC; COM decoding with code/data and entry annotations |
| 9. Additional assembler emitters | Implemented; validation expanding | Four flavors; three legacy emitters pass CPU, memory/location, alignment, console and FCB comparisons on both OS targets | Run the full arithmetic reference suite across all emitters; broaden real-program acceptance tests |
| 10. Real-program validation | Partial | Hello World, Microcosm CPU diagnostic, focused generated programs and native copy examples | Inventory new examples by CPU/dialect/API requirements; translate representative interpreters/games; record blockers and observable behavior checks |

## Immediate implementation order

1. File-service contracts and FCB handling: open, close, create, delete, rename,
   sequential read/write; then search, random records, size and zero fill.
   Establish compatible CP/M-86 paths first. DOS needs its own layout/status
   translation strategy before enabling corresponding calls.
2. Remaining console/device and disk/user services; pointer-returning calls and
   programs that inspect operating-system structures need explicit handling.
3. Finish the typed source representation and complete OS-operation annotations
   so recognition is not limited to an immediately preceding MVI C, constant.
4. Expand behavior tests and translate selected real programs, using their
   failures to prioritize verified gaps.
5. Add the remaining input and output formats independently.

These are implementation priorities, not permission to replace native
translation with a CP/M emulator or runtime compatibility layer.

## Completion criteria and evidence

For each BDOS or DOS operation, track separately:

- Contract researched: arguments, clobbers, return values, memory layout,
  version availability and target differences recorded.
- Lowering implemented: ordinary native target instructions, or an explicit
  source-port requirement with a useful diagnostic.
- Execution verified: input registers, result registers, memory effects,
  normal behavior and meaningful failure cases checked.
- Real use verified: exercised by a translated source program, when available.

A diagnostic-only/manual-port entry does not count as implemented support.
A handwritten target example does not count as automatic translation. In
particular, the handwritten native FCB copy examples demonstrate OS interfaces.
The separate tests/fileops.py suite now verifies automatic CP/M-86 translation
of open, close, create, delete, rename and sequential read/write. A restricted opaque-FCB subset is now translated to DOS and tested by tests/dosfcb.py. Added example files do not
count as passing programs until explicitly tested.

For CPU behavior, record coverage by instruction, operand form, flag inputs,
flag outputs and boundary conditions. For readers and emitters, require parsing
or assembly fixtures plus end-to-end execution checks where applicable.

Use make test as the regression gate. The current suite includes interface
ownership/source-immutability checks, expression evaluation, mnemonic fixtures,
both OS smoke/diagnostic suites, arithmetic execution, state validation and
layout/address-binding tests. Sanitizer checks cover the new expression,
state/layout and ownership paths. Emulator results do not establish behavior
of untested hardware interactions.

Do not publish one overall percentage: the milestones have very different
costs, and test coverage is not the same measurement as feature implementation.

## Completed file milestone

CP/M-86 functions 15, 16, 19, 20, 21, 22 and 23 now emit native INT E0h
sequences using the original CP/M sequential FCB. Tests translate 8080 source,
assemble/package it and execute under emu2. Copies of 0, 1, 127, 128, 129 and
260 records compare byte-for-byte; checks also cover EOF, missing-file open,
rename success/failure, deletion success/failure, and A=L byte results.
Disk-full, write-protection and device-error cases remain untested.

Next file milestones: zero-fill execution verification and DOS FCB
representation/search/random translation. M80 now has an initial flat-8080 reader; NASM, ASM86, MASM 3 and DRASM86 are implemented final
assembler emitters. Full M80 macro and module handling remains pending.

## Combined phase result

- CP/M-86 search (17–18) and random operations (33–36): implemented and executed.
- CP/M-86 zero-fill write (40): implemented and assembled; execution blocked
  by the installed emu2, whose dispatcher lacks this function.
- The incorrect native-FCB mode was removed. Input FCBs are always CP/M.
  Five sequential file lowerings now accept proven opaque-FCB uses, bringing
  restricted lowering coverage to 11/39 without a layout flag.
- General DOS CP/M-FCB conversion, DOS search/random translation, and
  zero-fill execution remain unfinished. This phase does not complete the
  full DOS compatibility objective.

## Next: CP/M FCB analysis and DOS lowering

1. Identify FCB objects, their storage bounds, aliases and uses at BDOS calls.
2. Classify name initialization, field reads/writes, copies and unknown escapes.
3. Generate native DOS storage and rewrite supported field accesses and call
   sequences, preserving the source operation's results and data behavior.
4. Diagnose unproven uses; never assume that CP/M bytes are DOS metadata.
5. Acceptance: the unchanged tests/copy.asm source translates with only
   --target=msdos and passes byte comparisons, EOF and extent-boundary tests.
   Add field-access and alias tests before claiming broader FCB support.

## Opaque-FCB milestone completed

The unchanged tests/copy.asm translates using --target=msdos alone. Its private
FCBs are proved to have dedicated storage and only bounded name initialization;
metadata is used exclusively by BDOS. fcb.c records approved call sites and
necessary storage growth. msdos.c lowers those approved operations natively.
Tests cover 33-byte reservations growing to 36, partial records, extent
boundaries, conditional create calls, unchanged source, safe existing-file
failure, missing files and rejection of unsafe uses. General field-access
rewriting, arbitrary alias/dataflow analysis and DOS search/random calls remain
unfinished. No source-layout assertion or compatibility runtime was introduced.

## Parser milestone

M80 reader subset added through --dialect=m80. It shares the source-record
interface and supports 8080 selection, listing directives retained as comments,
column-one operations and explicit indented labels. Both readers now support
CP/M EOF and CR/LF variants. Tests cover syntax/provenance, rejected unsupported
constructs, line bounds, normalized round trips and binary equivalence for
both targets. Syntax-only scans of the current Balloon, FIG-Forth, PILOT and
Tiny BASIC examples found no line-parser errors; this is not end-to-end program
translation coverage.

M80 conditional assembly and relative includes now expand before symbol/layout
analysis. Supported conditions are IF/IFT, IFE/IFF and IFDEF/IFNDEF, with nested
ELSE/ENDIF. Expanded records retain original file/line provenance and separate
stable ordinals for downstream analysis. Tests cover inactive branches, nested
includes, malformed blocks, include cycles, included-source overwrite protection,
stale state after included content changes, normalized round trips, binary
equivalence and included CP/M FCB source translated to DOS. Full regression and
address/undefined-behavior sanitizer checks of the preprocessing tests pass.
Basic MACRO/ENDM expansion now supports positional parameters, omitted and
angle-grouped arguments, nested calls and unique LOCAL labels. Definitions own
source text and provenance; expansion feeds the existing source IR. Tests compare
assembled bytes against explicit source for both targets and cover local-label
isolation, source/state round trips, included definitions, body-relative includes,
DOS FCB analysis after expansion, skipped definitions and bounded recursion.
The full strict-build regression and address/undefined-behavior sanitizer runs
for both macro and preprocessing tests pass.
REPT, IRP and IRPC blocks now support nesting and EXITM. Byte-equivalence
fixtures cover both targets, nested exit scope, included exits, inactive blocks,
zero counts, empty/list/character substitution and expansion limits. Counts use
known absolute values in 0..65535; IRP lists are limited to 32 items. Nested
parameter shadowing and LOCAL in anonymous blocks are explicitly diagnosed.
Macro &, ! and % operators plus IFB/IFNB/IFIDN/IFDIF are now implemented.
Tests compare assembled bytes on both targets and cover nested concatenation,
quoted substitution, escaped punctuation/whitespace, absolute evaluation at nested
calls, literal percent signs, malformed angle groups and included diagnostics.
Percent output uses the existing default decimal radix and low-16-bit values.
String comparisons preserve case and interior whitespace.
Mutable SET now snapshots values at each emitted use, including macro/repetition
and included source. Tests protect immutable names, forward-use diagnostics,
register/string operands, state invalidation and downstream DOS FCB recognition.
Word arithmetic/bitwise operators, HIGH/LOW and comparisons are implemented in
the shared expression IR. A Tiny BASIC dispatch-table fixture verifies the
multiplicative precedence of SHR. General arithmetic remains checked host-long
arithmetic; full M80 word-overflow semantics are not claimed.
NUL and .RADIX 2..16 now normalize in the M80 reader. Tests cover every radix,
source-order EQU/SET values, include/macro scope, percent formatting, inactive
changes, normalized/state round trips and diagnostics. Explicit numeric suffixes
retain priority; ambiguous high-radix percent results are diagnosed. General
M80 word-arithmetic and historical suffix differences remain unimplemented.
Next parser work: define and validate the dialect arithmetic contract with
reference fixtures. Nested parameter shadowing, pass conditions and relocatable
modules remain pending.
