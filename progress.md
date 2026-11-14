# xlt88 implementation progress

This ledger tracks the agreed design and implementation scope. Status describes
working code, not merely a design proposal. Update it when a milestone gains
implementation and tests. Detailed OS service coverage lives in API.md.

## Current measurements

| Measure | Current result | What it establishes |
| --- | --- | --- |
| 8080 mnemonic fixtures | 78/78 | Every translator mnemonic appears in an assembled fixture; not exhaustive instruction behavior |
| Arithmetic execution | 8,800 cases per OS target | Tested arithmetic/flag combinations agree with the reference model |
| CP/M-86 native BDOS lowering entries | 7/39 non-reserved CP/M 2.2 functions | Functions 0, 1, 2, 9, 12, 25, 26 have lowering code; not complete semantic coverage |
| DOS native BDOS lowering entries | 6/39 non-reserved CP/M 2.2 functions | Functions 0, 1, 2, 9, 25, 26 have lowering code; not complete DOS API coverage |
| Input readers | 1/3 planned families | Intel ASM implemented; MAC/RMAC and binary COM pending |
| Final assembler emitters | 1/4 | NASM implemented; MASM 3, ASM86 and DRASM86 pending |
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
| 5. Complete BDOS 2.2 to CP/M-86 | Early | Seven native lowering entries, including termination | Implement and validate remaining 32 source functions or document required explicit ports; far-pointer and system-data consumers need representation-aware translation |
| 6. Complete BDOS 2.2 to DOS 1.0 | Early | Six native lowering entries, including termination | Implement missing source semantics through native DOS operations; FCB layouts, statuses, user areas, attributes, drive policy and disk structures require explicit decisions, not call-number substitution |
| 7. Native DOS 1.0 operation support | Pending | Target version and local reference sources identified | Inventory native services; define source representation for DOS-only operations; verify each entry against DOS 1.0 rather than assuming later-version behavior |
| 8. Additional source readers | Pending | Intel ASM reader | MAC/RMAC syntax and macro handling; COM decoding with code/data and entry-point annotations; all feed the shared representation |
| 9. Additional assembler emitters | Pending | NASM renderer; normalized 8080 branch | MASM 3, ASM86, DRASM86, with exact version contracts, assembler runs and binary/execution equivalence |
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
particular, the native FCB copy examples currently demonstrate the OS interfaces,
not automatic translation of CP/M file operations. Added example files do not
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
