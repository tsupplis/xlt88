# xlt88 implementation progress

This ledger tracks the agreed design and implementation scope. Status describes
working code, not merely a design proposal. Update it when a milestone gains
implementation and tests. Detailed OS service coverage lives in api.md.

## Current measurements

| Measure | Current result | What it establishes |
| --- | --- | --- |
| 8080 mnemonic fixtures | 78/78 | Every translator mnemonic appears in an assembled fixture; not exhaustive instruction behavior |
| AND-immediate execution | 8,192/8,192: 1,024 per emitter/OS pair | Exhaustive accumulator values for four constant-AC masks, with result and full PSW checks |
| Arithmetic execution | 70,400/70,400: 8,800 per emitter/OS pair | All four emitters × DOS 1.0 and CP/M-86 agree with the reference model for the existing case set |
| CP/M-86 native BDOS lowering entries | 39/39; 27/31 require pointer-consumer port review | All non-reserved entries emit native code. Two return ES:BX without automatic source-pointer conversion; device, protection/user-state and zero-fill execution remain limited |
| DOS native BDOS lowering entries | 24/39, nine with validated FCB plans | 0–6, 9, 11, 13, 14, 19, 23, 25, 26 plus 15, 16, 20–22, 33–36 for audited private or first-default FCB uses; 3/4/5 device execution unverified; not general file/API coverage |
| Input readers | Intel implemented; M80/MAC partial | MAC normalises unchanged DISK7; RMAC and COM pending |
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
| 3. Typed intermediate representation | Partial | Fixed-region emission plan preserving source IDs; owned expression trees, ID-bound symbol leaves, checked arithmetic, EQU dependencies; typed target event/operand kinds | Typed source instructions/operands; structured target memory operands; full source provenance; remove remaining textual interpretation between passes |
| 4. Correct 8080 instruction behavior | Partial | 78 mnemonic fixtures, CPU diagnostic, arithmetic cases, PSW/DAA checks, both targets | Explicit instruction/operand/flag coverage matrix; fill untested combinations and boundary cases; distinguish executed checks from emitted-byte checks and hardware-only behavior |
| 5. Complete BDOS 2.2 to CP/M-86 | Native entry set complete; semantic validation partial | 39 entry-point lowerings; 27/31 retain native far results with mandatory review diagnostics | Automatic pointer-consumer translation remains absent; finish state/device/zero-fill execution validation and real-program coverage |
| 6. Complete BDOS 2.2 to DOS 1.0 | Early | Fifteen direct service entries plus nine validated FCB lowerings | Implement missing source semantics through native DOS operations; FCB layouts, statuses, user areas, attributes, drive policy and disk structures require explicit decisions, not call-number substitution |
| 7. Native DOS 1.0 operation support | Pending | Target version and local reference sources identified | Inventory native services; define source representation for DOS-only operations; verify each entry against DOS 1.0 rather than assuming later-version behavior |
| 8. Additional source readers | Partial | Intel ASM plus M80 8080 subset with conditions/includes/macros/LOCAL/repetition/EXITM/text operators/string conditions | M80/MAC remaining expression/word-arithmetic semantics/pass conditions/relocation; RMAC; COM decoding with code/data and entry annotations |
| 9. Additional assembler emitters | Implemented; validation expanding | Four flavors; three legacy emitters pass CPU, memory/location, alignment, console and FCB comparisons on both OS targets | Arithmetic matrix complete; broaden real-program acceptance tests |
| 10. Real-program validation | Partial | Repeatable `make corpus` inventory of nine assembly sources; Hello World, CPU diagnostic and explicit console-only Forth port execute in all eight combinations; per-stage report in corpus.md | Forth disk/printer port; PILOT monitor hooks and target expansion; Tiny BASIC restart bodies/DOS FCB proof; Balloon hardware port |

## Immediate implementation order

1. File-service contracts and FCB handling: open, close, create, delete, rename,
   sequential read/write; then search, random records, size and zero fill.
   Establish compatible CP/M-86 paths first. DOS needs its own layout/status
   translation strategy before enabling corresponding calls.
2. Remaining console/device and disk/user services; pointer-returning calls and
   programs that inspect operating-system structures need explicit handling.
3. Finish typed source operands and extend OS-operation analysis to
   broader memory-loaded selectors, saved registers and explicit subroutine effects.
   Dedicated DS 1 selectors now have bounded candidate recognition and runtime guards.
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

## Arithmetic matrix and example baseline

The full arithmetic matrix passes 70,400 result/PSW comparisons: four emitters,
two OS targets, 8,800 cases each. Historical MASM 3 runs out of memory on
large DAA batches; all emitters now use the same 64-case batches. No cases
were dropped. `tests/toolrun.py` shares strict assembler/build validation with
the emitter fixtures. This expands validation across renderers; it does not
make the arithmetic input space exhaustive.

`make corpus` records normalization, translation/rendering, assembly and
explicit execution separately in [corpus.md](corpus.md). Six 8080 inputs are
tracked; four existing 8086 reference outputs are excluded. Two inputs execute
across all eight combinations. Balloon only builds with NASM and remains
hardware-specific. Three interpreter sources are blocked. Normalization is
not a claim of instruction support, and successful assembly is not a claim
of safe or correct execution.

The M80 `IFF` label/directive ambiguity is fixed with focused regressions.
FIG-Forth now normalizes unchanged; its next observed blocker is the `NEXT+3`
transfer. Resolve source offsets to instruction boundaries before emitting
target labels; never carry the source byte offset into translated machine code. PILOT and Tiny BASIC
need native source-aware translation work, not a compatibility runtime.

## Combined translation milestone

- Source-offset branches/calls now bind unique original instruction boundaries.
  FIG-Forth translates and assembles unchanged with all four emitters on both
  targets. This remains an assembly milestone: native BIOS calls and threaded
  dictionary layout require execution-focused porting before claiming a usable
  interpreter.
- Forward fixed ORGs are supported with an explicit target origin event and
  assembler overflow guards. At this earlier milestone PILOT's data-first
  region order was unsupported; fixed-region ordering is implemented below.
  Its external monitor hooks still require a port.
- BDOS C recognition now survives intervening preserving instructions and also
  recognizes LXI B constants. It remains conservative at labels, calls, branches
  and unknown/clobbering instructions. Tiny BASIC's omitted restart bodies and
  return-address/inline-data conventions remain separate blockers.
- Native CP/M-86 buffered console input and native input status on both targets
  are implemented. At that milestone coverage was 24/39 CP/M-86 and 12/39 DOS. DOS buffered
  input needs buffer adaptation; its different terminator capacity is not ignored.

The four workstreams advanced together; interpreter execution, arbitrary region
reordering, general dataflow and complete BDOS support are not complete. The console-only Forth acceptance target is completed below; broader interpreter and OS coverage remains open.

## Console-only Forth execution

`tests/figforth.py` derives an explicit BDOS console port while leaving
`examples/forth.asm` unchanged. All four assemblers on both targets pass startup,
`123 456 + .` (579), defining `SQUARE`, `7 SQUARE .` (49), and `BYE`.
Actual disk/printer BIOS requests terminate with an explicit unsupported message;
this is not a complete Forth disk port or an exhaustive interpreter test.

BDOS 6 now lowers to native operations on both targets. CP/M-86's special FE
status mode is avoided for CP/M 2.2 FE character output. Coverage is 24/39 and
13/39 respectively. The regression tests cover ready status, direct input with
A=L, ordinary output and FE output, using emu2 keyboard scripts with a terminal
input handle and its 8088 CPU mode.

The Forth run exposed per-initializer `$` semantics: MACRO-80 assigns different
addresses to `DW $, $`. Data lists now emit separate typed items so every
assembler observes the correct location. Tests check both word and byte lists,
including quoted commas. All these checks are part of `make test`.

## Next major step: source regions and target layout

The console-only Forth fixture is an adapted test input, not proof that original
BIOS-dependent programs translate automatically. The production changes in this
batch are native BDOS 6 lowering and correct data-initializer location counters.

Next, represent source regions explicitly and separate source addresses from
target placement. This addresses PILOT's out-of-order ORGs and VTL-2's fixed
ORG region overflow after instruction expansion.

1. Record region origins, contents, ordering and symbol references in the IR.
2. Distinguish fixed-address constraints from relocatable placement explicitly;
   preserve fixed origins by default and diagnose overflow. Never silently move
   hardware addresses or reinterpret source address arithmetic.
3. Let CPU lowering produce target regions and let each emitter render the same
   placement decisions. Reconcile source and target symbol addresses explicitly.
4. Validate boundary/overlap/overflow cases and all four assemblers on both OS
   targets. Re-measure PILOT and VTL-2; monitor hooks and DOS FCB proof remain
   separate execution requirements.

Acceptance: supported symbolic multi-region programs build consistently in all
eight combinations, fixed-address violations remain explicit, and the example
report distinguishes layout progress from actual execution.

For the next batch, use the user's working ASM86 translations as independent
comparison references for the newly added tools. Inventory their input dialects
(including MAC), compare symbol/data layout and native OS calls, and select the
first reference-backed acceptance case before extending translation. Adding a
reference file does not establish that its source currently builds or executes.

## Batch sign-off — 2026-11-14

Full `make test` passed, including 70,400 arithmetic cases across all eight
emitter/OS combinations, console/Forth acceptance, symbol state, source layout,
CP/M-86 sequential/random/search operations and DOS opaque-FCB translation.
The console and Forth suites also passed with address/undefined-behaviour
sanitizers; Forth was rerun successfully after moving its fixture generator into
`tests/`. BDOS 40 remains emission-only because the installed emulator does not
implement it; this unchanged gap is not counted as executed coverage.

The next batch starts with `vtl2.asm`, `wash15.asm`, and `disk7.mac` and their
working `.a86` references: VTL-2 is direct xlt86 output; WASH and DISK7 include
manual changes. First compare
source, reference and (when available) patches; classify differences as reader,
CPU translation, source/target layout, or explicit OS/hardware porting. Use that
evidence to choose the first implementation slice of the region-layout plan.
A working reference is an acceptance oracle, not a replacement for translation.

## Reference and DOS batch — implementation

- Native DOS delete and rename added: source BDOS coverage is 15/39. Four-emitter
  execution covers success/missing cases, wildcard deletion, A=L and guard bytes.
  Rename collision execution is unverified because emu2 overwrites destinations;
  `DOS_RENAME=1` enables the strict acceptance check on a corrected emulator.
- Arithmetic native-service aliases (BDOS=5+BASE, WBOOT=0+BASE) now resolve.
- Fixed out-of-order regions now have an owned emission plan. Original record
  indices, symbol identities and origins remain intact. Eight-combination tests
  cover pointers, branches and execution, with rejection of overlaps/unknown
  sizes/missing entry regions. Expansion overflow is not silently relocated.
- All three supplied references assemble without errors under ASM86 1.1N2.
  Findings and remaining program-specific issues are documented in refs.md.
- Corpus measurement now includes wash15.asm and disk7.mac. MAC remains an
  explicitly unsupported reader, not an M80 alias. None of these three complete
  programs is claimed as executing under xlt88 yet.

## Reference/DOS batch sign-off and next batch

Full `make test` completed successfully: 70,400 arithmetic cases, all existing
state/layout/file suites, and the new region/alias/name-service checks. The new
focused suites also passed address/undefined-behaviour sanitizers. BDOS 40 and
DOS rename collisions retain their documented emulator execution gaps.

Next batch: unchanged VTL-2 versus its direct xlt86 reference. Establish a
repeatable reference execution transcript, compare code size and segment/layout
contracts, and identify the general cause of the fixed-0800h overflow. Implement
only a justified shared translation/layout correction, then compare execution
and rerun relevant emitter/CPU checks. No source edits, program-name exceptions,
or invented operating-system state count as success. MAC parsing is a separate
general reader task; the WASH/DISK7 patches guide requirements and feasibility,
not automatic adoption of program-specific fixes.

## VTL-2 benchmark and drive selection

`make vtlbench` assembles and executes the supplied direct xlt86 reference in
8088 mode and measures unchanged source translation for every emitter/target.
Reference startup, 123+456=579, A=7 then A*6=42, and exit pass. Its code boundary
is 078Bh before variables at 0800h. Current xlt88 NASM code exceeds that fixed
boundary by 513 bytes; the other CP/M-86 emitters also fail placement, and DOS
additionally rejects unproved default-FCB use. See vtlbench.md for hashes,
transcript and per-target diagnostics. These are measured blockers, not xlt88
execution passes. No source or reference bytes were changed.

BDOS 14 is now implemented on both targets and passes eight-combination drive
selection/current-drive/default-drive file tests. Coverage is 25/39 CP/M-86 and
16/39 DOS. BDOS 13 remains blocked after native-contract review. Further VTL-2
work requires a measured, semantics-preserving code-size improvement or an
explicitly justified layout contract; removing the fixed origin is not accepted.

Validation for this additive service change: drive-selection execution passed
all eight combinations, and CP/M-86 sequential file, DOS opaque-FCB and DOS
name-service regressions passed. The CPU lowering did not change; the preceding
70,400-case full regression remains the CPU baseline. The VTL-2 benchmark
completed and explicitly recorded all eight xlt88 combinations as blocked
before execution, rather than counting them as passing programs.

## General branch-size improvement

The shared target pass now proves short-branch reach using conservative 8086
encoding bounds. It folds inverse-condition/jump sequences when safe and stops
legacy emitters from expanding proven internal short skips a second time.
Unknown instruction sizes, data, alignment and origin events remain barriers.
No source-specific handling, CPU flag shortcuts or source changes are involved.

| VTL-2 emitter | Previous excess over 0800h | Current excess | Bytes saved |
| --- | --- | --- | --- |
| NASM | 513 | 413 | 100 |
| ASM86 | 835 | 433 | 402 |
| MASM 3 | 812 | 422 | 390 |
| RASM-86 | 835 | 433 | 402 |

The remaining excess is still a blocker. The xlt86 reference execution passes;
no xlt88 VTL-2 execution is claimed. Dedicated tests cover branch bounds, layout
barriers, repeated optimisation and near/backward/far branch execution across
all eight combinations. The full regression passed, including 70,400 arithmetic
cases. Focused branch/control-flow sanitizer checks also passed.

## MAC parser batch

The explicit `--dialect=mac` reader now normalises unchanged DISK7. Independent
fixtures assemble and execute across DOS and CP/M-86 with all four emitters.
They cover macros/conditions, versioned forward SET expressions, embedded dollar
separators, quoted bytes and labelled directives. Invalid identifiers, unknown
conditional values, unsupported entry points and intermediate word-wrap cases
fail atomically. See [mac.md](mac.md) for the supported subset.

DISK7 remains blocked at native translation by unsupported/dynamic BDOS calls;
normalization is not a program execution pass. Complete MAC/RMAC, far-pointer
provenance and finite-valued BDOS dispatch remain separate work. Native service
coverage remains CP/M-86 25/39 and DOS 16/39; this parser batch adds no services.

Validation: shared M80 reader/preprocessor/macro/repetition/expression suites,
region/layout/state tests and all eight MAC native combinations passed. MAC,
preprocessing and SET tests also passed address/undefined-behaviour sanitizers
with a strict warning-free C99 build. The corpus report has been regenerated.

## Bounded BDOS analysis and native device batch

- Register-copy, immediate, increment/decrement and direct control-flow analysis
  now propagates up to four service values. Direct-call inputs merge at shared
  routines; call return registers remain unknown.
- Native dispatch selects only proven service alternatives and restores the
  incoming flags before each native service. Unknown or unsupported alternatives
  fail atomically. Set overflow becomes unknown rather than dropping values.
- DOS FCB recognition accepts intervening proven-safe register operations between
  the DE address load and service call, retaining the existing metadata audit.
- Auxiliary/list output (4/5) has native lowering on both targets. All eight
  combinations assemble; device execution is unverified. Counts are now 27/39
  CP/M-86 and 18/39 DOS, measuring lowering rather than execution coverage.

Both dispatch alternatives, shared wrappers and register copies execute across
all eight combinations. FCB copy tests include nonadjacent service setup,
partial records, extent boundaries, no-clobber and unsafe-access rejection.
The corpus retains its previous translate/assemble/execute totals. WASH's OUTOP
and DISK7's CON$LST are memory-loaded selectors and remain unknown; memory-value
analysis is not implemented in this batch. DISK7 gets past function 5, but reset,
user area, segmented pointers and its extended function 46 remain blockers.

Next substantial work: conservative analysis of stored service selectors,
stack-saved register values and explicit subroutine effects; separately, VTL-2
code size and pointer provenance. No complete WASH/DISK7 execution is claimed.

Batch validation completed: full regression passed, including all 70,400
arithmetic cases and the remaining file/API suites. After modular cleanup,
focused control-flow, dispatch, FCB and corpus checks passed again; the injected
resolver unit test and dispatch/FCB paths passed sanitizer checks. Separated FCB
setup also executes through all four DOS emitters. Device calls assemble in all
eight combinations but remain execution-unverified. VTL-2 measurements are
unchanged; its reference still passes and xlt88 still exceeds the fixed boundary.

## VTL-2 execution milestone

Unchanged VTL-2 now fits its fixed 0800h variable boundary and passes the
reference transcript on CP/M-86 through NASM, ASM86, MASM 3 and RASM-86:
startup, 123+456=579, assignment A=7, A*6=42, subtraction=667,
wide multiplication=65025, division=257, 16-bit wrap=0, and exit. The source and xlt86
reference hashes remain unchanged. This establishes the tested transcript,
not exhaustive language or script-file functionality. DOS remains blocked by
unproved default-FCB handling. See vtlbench.md for each target's actual result.

General changes responsible for this milestone:

- Owned per-record flag-liveness masks, with conservative unknown edges and
  returns, track individual 8080 flags across direct branches and callee inputs.
- CPU lowering omits only proven-dead flag maintenance. Native word INC/DEC
  retain carry, and native ADD implements DAD when other flags are dead.
- AND-immediate masks with bit 3 set use the constant 8080 auxiliary-carry result.
- Safe adjacent word-step sequences share a canonical saved flag image.
- Proven short unconditional jumps are explicit in the legacy emitters;
  conservative instruction-size bounds now account for short register forms.

Reader/auxiliary input (3) now has native lowering on both targets and assembly
checks in all eight combinations; device execution remains unverified. CP/M-86
login-vector (24) passes native execution through all four emitters, checking
both low and high bitmap bits. Lowering counts are 29/39 CP/M-86 and 19/39 DOS.
No DOS login-vector semantics have been invented.

Quality review consolidated flag analysis into one documented, immutable-input
interface, removed superseded helpers/state, reviewed event ownership and range
proofs, and added isolated graph/PSW tests. The AND matrix adds 8,192 result/PSW
cases, distinct from the existing 70,400-case arithmetic matrix.

Final validation for this batch: the frozen implementation passed the full
regression, including 70,400 arithmetic and 8,192 AND/PSW cases, all existing
file/API suites, and four-emitter CP/M-86 login-vector tests. The extended VTL-2
reference comparison passed on all four CP/M-86 emitters and is now included in
`make test` as a required acceptance gate. Focused flag/branch/AND sanitizer
runs, isolated ownership/graph sanitizer checks, strict warning-free compilation
and static analysis of flags.c/shrink.c passed. Final code review checked flag
read/write contracts, unknown-edge conservatism, conditional-call paths, AH
ownership during coalescing, and assembler range bounds. Generated test binaries
are excluded from source changes.

## Script acceptance and additional native services

Unchanged VTL-2 and its unchanged xlt86 reference now agree on empty, missing,
stored-program, multi-record and Ctrl-Z-terminated script files, including a
CR/LF record-boundary split. All four CP/M-86 emitters pass `tests/vtlscr.py`.
DOS VTL-2 remains blocked by default-FCB representation and field-use proof.

`fcbaddr.c` separates straight-line DE address discovery (including LXI H/XCHG)
from whole-program metadata auditing. Default addresses 005Ch/006Ch are now
recognized and diagnosed precisely; recognition alone does not authorize
conversion. Independent analysis tests and four-emitter file-copy tests cover
this boundary.

CP/M-86 adds attributes (30), user area (32), drive protection (28) and read-only
bitmap (29). DOS adds audited random read (33) and set random record (36).
Current lowering coverage is 33/39 CP/M-86 and 21/39 DOS, not a claim of complete
execution coverage. Attribute read-only changes, zero read-only bitmap, DOS
bookmarks/repeated reads/extent crossing/padding/EOF execute on all four emitters.
User/protection state and DOS sequential continuation after a random read remain
explicitly unverified on the installed emulator; see api.md for strict checks.

Validation: the full `make test` gate passed after these service additions,
including 70,400 arithmetic cases, 8,192 AND/PSW cases, existing file/API tests,
new random/protection tests and four-emitter VTL-2 script/reference comparisons.
Strict C99 warning checks, Clang static analysis of the changed C modules and
ASan/UBSan execution of both new suites passed. `make corpus` was refreshed;
no previously blocked real program is reported as newly executing.

## Native default-FCB milestone

The agreed native-memory boundary removes the DOS VTL-2 blocker without a
compatibility runtime or source-specific changes. The first FCB uses DOS PSP
storage directly. Explicit incompatible field accesses are diagnosed by a small
independent pass; arbitrary computed memory sees native target structures.
This replaces the earlier requirement to prove that an interpreter can never
access OS metadata. It does not weaken the private-FCB relocation audit.

Unchanged VTL-2 passes interactive calculations and all five script-file cases
on DOS through NASM, ASM86, MASM 3 and RASM-86. The original xlt86 reference
remains the baseline; only CP/M's additional final warm-boot CR/LF is excluded
from the DOS comparison. Both targets are now required benchmark gates.
Second default FCBs, mixed default/private file objects and explicit incompatible
metadata accesses remain diagnosed. Service counts remain 33/39 and 21/39;
this milestone expands usable programs rather than adding service numbers.

Validation for this milestone: both-target/four-emitter VTL-2 interactive and
script comparisons passed, alongside default-FCB positive/negative cases,
private FCB copy and random-record regressions, service-set tests, interface and
address-analysis tests, and the refreshed corpus. Strict C99 compilation,
Clang analysis and focused ASan/UBSan runs passed. The CPU emitter was unchanged;
the previously passed full arithmetic matrix was not rerun for this analysis-only
change. Test fixtures preserve the original examples and xlt86 reference.

## Earlier batch: WASH reference and three native service additions

The supplied patched WASH15 reference now has a repeatable acceptance gate:
missing-file handling, startup/exit, browsing, help and viewing. Fixture files
remain unchanged. Original-source translation is still blocked, separately
recorded in wash.md: memory-loaded service selection and disk-reset semantics
on CP/M-86; DOS also needs directory/FCB representation work. The reference's
interrupt-enable patch is a deliberate policy change, not an automatic CPU
translation rule. No WASH-specific source patch or translator exception was added.

DOS random write (34) and file size (35) now have native lowering, direct reads
of compatible random-record fields, and all-four-emitter execution tests on
private/default FCBs. Tests check repeated overwrites, extent crossing, missing
files, rounded sizes and the third count byte. CP/M-86 selected-drive reset (37)
has native lowering and all-four-emitter assembly validation; emulator support
still blocks state execution. Counts at that stage were 34/39 CP/M-86 and 23/39 DOS.

Quality changes share the supported FCB service set between analysis and
lowering, and resolve arithmetic BDOS aliases consistently. Expression-based
field reads preserve relocatable symbol identities; numeric-address shortcuts,
incompatible writes and boundary-crossing reads remain rejected. New tests
exercise `BDOS EQU 5+0` through default and private storage paths.

Final validation: `make test` passed for this batch, including 70,400 arithmetic
cases, 8,192 AND/PSW cases, all new service tests, both-target/four-emitter VTL-2
interactive/script acceptance and five WASH reference scenarios. Strict C99
warning checks, Clang static analysis and focused ASan/UBSan runs passed.
The corpus report was regenerated. CP/M-86 reset state and DOS allocation-error
execution remain explicitly unverified; no WASH translated execution is claimed.

## Guarded selectors, reset and WASH acceptance

Memory-loaded service candidates come from known direct stores to a dedicated
DS 1 cell, independent of program and symbol names. They are not alias proofs:
every candidate, including a singleton, is checked at runtime. An unmatched
value terminates through the native target exit, with that limitation diagnosed
at translation time. Unknown direct stores and unsupported candidate services
remain errors. Guarded facts cannot authorize DOS FCB representation conversion.

BDOS 13 now uses native reset, drive selection, DMA and search on both targets.
Tests cover repeated calls, A selection, DMA 0080h, ordinary/$-prefixed files,
result registers and unchanged file contents across all eight combinations.
Physical media and read-only state remain unverified. CP/M-86 7/8 lower to native
IOBYTE calls and assemble with all emitters; routing-state execution is unverified.

Unchanged WASH15 matches five complete reference transcripts (missing file,
exit, browse, help and view) on all four CP/M-86 emitters. DOS remains blocked
by FCB representation analysis. Copy/delete/rename and device scenarios remain
to test. Its original interrupt-disable policy is preserved; emulator transcript
agreement does not validate real-hardware interrupt behavior. Difficult semantic
ports remain explicit post-translation work, never application-name heuristics.

Validation: the full `make test` gate passed, including 70,400 arithmetic and
8,192 AND/PSW cases and both VTL-2 acceptance suites. A subsequent correction
made the reset search cover all CP/M extents/modules; reset (eight combinations)
and WASH (four emitters, five scenarios) were rerun successfully on the final
normal and ASan/UBSan builds. Strict C99 compilation and Clang analysis passed.
The corpus was refreshed; WASH now assembles in 4/8 combinations. Its execution
is measured separately by tests/wash.py. See porting.md for the manual-review
checklist, especially implicit page alignment and segmented-pointer assumptions.

## Native entry completion and reference comparison

BDOS 27/31 now emit their native ES:BX results, with diagnostics and assembly
comments requiring pointer-consumer review. Four-emitter ABI tests use explicit
segment-aware target adjustments and compare against direct native calls.
This completes the native entry set, not automatic translation of every source
memory contract. No application-name heuristic or implicit segment switch was
added. The previous rule excluding diagnostic-only entries still applies:
these calls now emit and execute real native operations, but their consumer
port requirement is tracked separately from entry coverage.

WASH's five exact transcript comparisons now also record CMD sizes relative to
the supplied reference. DISK7's supplied reference has four repeatable scenarios
(exit, browse, length and view), while unchanged translation still diagnoses
two dynamic selectors and CP/M 3 function 46. Its 27/31 consumer warnings remain
visible. There is no xlt88 DISK7 executable comparison yet. See diskref.md.

Regression policy: retain independent CPU/API/parser/emitter tests and add
reference behavior comparisons. Raw assembly differences and CMD size are
review/efficiency evidence, not semantic equivalence tests. Patched references
must identify deliberate policy changes, especially interrupt handling.

Validation for this batch: full make test passed, including 70,400 arithmetic,
8,192 AND/PSW cases, VTL-2 interactive/script comparisons, WASH's five exact
transcripts and DISK7's four repeated reference scenarios. tests/bdosall.py
passed separately through all four emitters and is wired into future gates.
Strict C99 compilation, Clang analysis and focused ASan/UBSan tests passed;
make corpus was refreshed. The global pause-point assessment is in status.md.


## CP/M-86 pass-through and WASH file comparisons

The optional native backend hook now handles unknown service selectors,
unsupported known numbers and unmatched guarded candidates on CP/M-86.
Diagnostics and short assembly comments make the native-contract boundary
explicit. Known calls retain their adaptations; DOS retains rejection or
unmatched-candidate termination. This is not added automatic service coverage.
Tests distinguish known/dynamic 6/13 behavior, tail returns, native extension
continuation and known/dynamic ES:BX results. No program-name rules were introduced.

WASH has seven additional all-four-emitter transcript/file comparisons:
rename, cancel, duplicate rejection, delete, overwrite refusal, same-drive
rejection and invalid-drive handling. Copy probes are reported independently:
the reference and NASM time out; other emitters complete with matching bytes.
The source retains a CP/M-80 memory-limit lookup and unchecked buffer-pointer
growth, so a native allocation-aware port remains explicit work.

DISK7 now translates through all four emitters and assembles with NASM, MASM3
and DRASM86. ASM86 times out; generated execution is not claimed. Four working
reference scenarios remain separate evidence. DOS WASH analysis is in washdos.md:
standalone DS declarations, bounded copy effects, explicit FCB fields, directory
results, buffered input and native allocation each need their own treatment.


Current batch validation: make test completed successfully, including 70,400
arithmetic/result/PSW and 8,192 AND/PSW cases, VTL-2 on both targets, all twelve
WASH comparison scenarios, and the DISK7 diagnostic checkpoint. The latter
records an ASM86 timeout rather than claiming assembly success. WASH copy
probes record reference/NASM timeouts; ASM86/MASM3/DRASM86 additionally pass
513-record copies with exact contents and source preservation.

Strict C99 compilation, Clang analysis and focused ASan/UBSan native-call and
selector tests passed. The final extension-continuation tests passed without
assuming that emu2 rejects function 46. Wrapper timeouts now stop the spawned
process group; a focused timeout check and the final checkpoint exercised it.

## ASM86 repair and bounded WASH port checks

The external ASM86 banner loop was reduced to a nested EQU expression and
traced to its 600-byte private evaluator stack. A 2048-byte stack fixes the
assembler; the installed cpm86_asm86 now assembles generated DISK7 with zero
errors. DISK7's four-emitter assembly checkpoint now fails on any rejection or
timeout rather than treating those outcomes as informational.

All non-NASM emitters, including normalized 8080, now write CRLF plus one final
Ctrl-Z. Byte-level regression and normalized-source round trips cover this.
Test-only target adjustments preserve the selected emitter's file format.

The explicit WASH port checks pass 18 copy/replace cases per emitter (72 total),
plus oversized-directory rejection and unchanged-source assertions. Empty,
one-record and 513-record files are checked by exact bytes. Reduced allocation
limits are simulated base-page descriptors; canaries verify the next 16 bytes
are unchanged. This is test-only native port acceptance, not automatic WASH
translation or real CMD allocation acceptance. CMD sizing policy is deferred.

Next functional work: DOS buffered input (BDOS 10), then generic declaration
discovery for FCB labels separated from their storage records. Directory-result
conversion and bounded subroutine effects remain subsequent DOS blockers.

The next batch also includes a first usable annotated 8080 COM reader, currently
planned rather than partially implemented. Its acceptance boundary is exact
byte decoding at origin 0100h, explicit code/data and pointer decisions, editable
address labels and small end-to-end fixtures through both targets/all emitters.
It does not promise automatic recovery of arbitrary binaries.

Validation complete: full `make test` passed with the repaired ASM86, including
all four DISK7 assembly checkpoints. The WASH explicit-port checks passed
separately and are registered for future full gates. Corpus regenerated;
DISK7 assembly is now 4/8 combinations. Original examples/references unchanged.
The corpus and status reports are refreshed. Original examples and references
remain unchanged. This batch is complete; no next-batch implementation started.
