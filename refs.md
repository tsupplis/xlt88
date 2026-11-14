# Working ASM86 reference comparison

VTL-2 is a working direct xlt86 translation and is the primary xlt86-versus-xlt88
benchmark. WASH and DISK7 are working targets after the supplied manual changes.
All three assembled with zero errors using ASM86 1.1N2. VTL-2 reference execution
now passes startup, arithmetic, variable assignment and exit under the repeatable
`make vtlbench` benchmark; see vtlbench.md. WASH and DISK7 were not executed.
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
| examples/vtl2.asm | examples/ref/vtl2.a86 | 809db9dd5d2b | Translation expands beyond the fixed ORG 0800h variable block; DOS also rejects unproved default-FCB handling |
| examples/wash15.asm | examples/ref/was15.a86 | 51fc25cb6fd9 | Arithmetic BDOS/WBOOT aliases now resolve; dynamic function selection and disk reset remain blocked |
| examples/disk7.mac | examples/ref/disk7.a86 | a1c87a89c80f | MAC subset normalises unchanged source; native BDOS lowering still blocked |

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

At OUTCM5, WASH loads OUTOP into A, copies A into C and calls BDOS. The selected
service is therefore a program value, not a statically known immediate. The
current conservative recognition pass correctly refuses to guess. The source
also requests BDOS 13 (disk reset), which remains unimplemented on CP/M-86.
DOS additionally needs proof or explicit translation of this program's FCB
field handling. WASH uses inline message data after calls; successful assembly
alone will not establish those return-address semantics.

## DISK7: actual MAC input

The source selects the 8080 branch with `Z80 EQU FALSE`; its Z80-named macros
have 8080 alternatives. Syntax includes `7$7`, embedded dollar signs in names,
`$-MACRO` listing control, macro parameters and a label on ORG. The explicit MAC reader now handles these forms and normalises the unchanged
source. Independent fixtures exercise the same rules across all emitters.
The reference remains a comparison target; no preprocessing patch or reference
assembly is substituted for translated source. Native lowering still reports
unsupported or dynamically selected BDOS calls, including pointer-returning
services requiring segment provenance.

## Batch outcome and next acceptance

This comparison led to two shared production fixes: arithmetic service aliases
and fixed-region ordering. Independent fixtures execute both changes across
all four emitters on DOS and CP/M-86. PILOT also gets past its data-first region
ordering and now reports unresolved monitor hooks. No new full-program
execution claim is made for VTL-2, WASH, DISK7 or PILOT.

The next implementation slice should use unchanged VTL-2 and its direct xlt86
reference as the first program acceptance target. MAC parsing, finite-valued BDOS
dispatch and segmented/aligned layout are distinct requirements; implement and
measure each rather than disguising program-specific patches as general support.
