# Working ASM86 reference comparison

VTL-2 is a working direct xlt86 translation and is the primary xlt86-versus-xlt88
benchmark. WASH and DISK7 are working targets after the supplied manual changes.
All three assembled with zero errors using ASM86 1.1N2. VTL-2 reference execution
now passes startup, arithmetic, variable assignment and exit under the repeatable
`make vtlbench` benchmark; see vtlbench.md. WASH now passes five reference
transcript comparisons with unchanged translated source on four CP/M-86 emitters;
DISK7 has four repeatable reference-only scenarios (diskref.md); its unchanged source now translates; assembly and port readiness are recorded
separately in diskref.md.
These are comparison inputs, not xlt88 output.

The supplied DISK7 preparation script compensates for xlt86 reader limitations:
macro expansion, ELSE, labelled ORG/END, forward SET, reserved names and long
strings. These are requirements for general reader/emitter handling, not a
script to adopt as a DISK7-specific workaround. Its target patches select the
program stack segment and use ES for BDOS 27/31 far-pointer consumers.
WASH's target patches select SS, re-enable interrupts, and access the PNAME
buffer through DI to avoid ASM86 label typing. Re-enabling interrupts changes
the original DI policy; it is not a universal translation rule. Far-pointer
consumers require proven segment provenance. Unsupported cases must remain
explicit rather than being patched by program name.

| Original source | Reference | Reference SHA256 prefix | Current xlt88 finding |
| --- | --- | --- | --- |
| examples/vtl2.asm | examples/ref/vtl2.a86 | 809db9dd5d2b | Interactive and script acceptance pass on both targets, all four emitters; fixed ORG 0800h retained |
| examples/wash15.asm | examples/ref/was15.a86 | 51fc25cb6fd9 | Five unchanged-source transcript scenarios pass on CP/M-86, all four emitters; DOS FCB analysis remains blocked |
| examples/disk7.mac | examples/ref/disk7.a86 | a1c87a89c80f | Reference scenarios pass; source translates; NASM/MASM3/RASM-86 assemble, ASM86 times out; native pointer consumers need review |

## VTL-2: address constraints and segments

Both sources explicitly place `vars` at 0800h. The comment explains the reason:
`convp` uses an eight-bit addition without propagating carry. Therefore moving
the variable block to an arbitrary address would be incorrect. The reference
also switches CSEG/DSEG, whereas xlt88 currently emits a combined image. Matching
a working segmented reference requires understanding its load/segment contract,
not deleting the ORG or copying its numeric offsets into larger generated code.

The new region planner orders fixed regions while retaining their origins. It
does not solve expansion overflow by relocation. Explicit alignment/placement
constraints or a segmented target representation remain separate future work.
The reference also establishes SS before selecting SP; xlt88 already has a
CP/M-86 stack-segment hook. Low-memory TPA assumptions and script-file handling
must be checked during execution acceptance.

## WASH: service recognition and program semantics

`BASE EQU 0`, `BDOS EQU 0005H+BASE`, and `WBOOT EQU 0+BASE` are now resolved as
native service entries. Previously those arithmetic aliases were rejected.
The change does not permit arbitrary source offsets into expanded target code.

At OUTCM5, WASH loads OUTOP into A, copies A into C and calls BDOS. Generic
DS 1 store/load recognition now supplies guarded candidates; every candidate
is checked at runtime, with native termination for an unmatched value. No alias
proof or application-specific names are assumed. BDOS 13 uses native reset,
drive/DMA selection and search. DOS still needs explicit FCB representation work.
Five complete transcripts now exercise inline-message return-address behavior
and code-label data access, not just assembly. Original DI remains unchanged;
the reference's added STI is a manual policy change, and hardware behavior is
not established by the emulator comparison.

## DISK7: actual MAC input

The source selects the 8080 branch with `Z80 EQU FALSE`; its Z80-named macros
have 8080 alternatives. Syntax includes `7$7`, embedded dollar signs in names,
`$-MACRO` listing control, macro parameters and a label on ORG. The explicit MAC reader now handles these forms and normalises the unchanged
source. Independent fixtures exercise the same rules across all emitters.
The reference remains a comparison target; no preprocessing patch or reference
assembly is substituted for translated source. Native lowering now warns and passes through dynamically selected calls and
CP/M 3 function 46. Native 27/31 calls now
emit ES:BX with explicit consumer-review diagnostics. Their segment provenance
is not inferred. tests/diskref.py records the remaining blockers and reference
scenarios; no generated DISK7 execution is claimed.

## Next acceptance

VTL-2 supplies interactive and script-file acceptance on both targets through
all four emitters. WASH supplies five unchanged-source CP/M-86 scenarios; expand
to file mutation and devices, and examine DOS FCB representation separately.
DISK7 still needs pointer/segment provenance; PILOT needs monitor ports. MAC
parsing, service recognition and segmented layout remain distinct requirements.
Difficult cases may need explicit post-translation edits. Reference patches
must never become application-specific translation rules.


## What the supplied xlt86 output demonstrates

DISK7's TABL/NOTAB sites use a directly emitted INT 224 after loading the
service selector from memory. The reference also retains GETVERS, the branch
to FREE20, and the function-46 INT 224 in the other branch. Thus these sites
are passed through to the native OS rather than rejected for lack of a proven
selector or for lying outside CP/M 2.2. This is visible in the supplied target
reference; the local crossdev xlt86 command reference also documents CALL 5
as INT 224.

The user's original-to-patched diff shows that xlt86's original 27/31 consumers
used ordinary near accesses. ES overrides were manual additions. The working
reference therefore demonstrates a successful explicit port, not automatic
segment-provenance analysis by xlt86.

This motivated the native CP/M-86 pass-through policy with review warnings
for unknown selectors and native extensions. It retains native contracts
without promising CP/M-80 semantics; functions 6 and 13 demonstrate why that distinction matters. DOS
still requires service-specific lowering. This policy is now implemented through an optional backend hook; known
service adaptations remain in place and DOS remains strict.
