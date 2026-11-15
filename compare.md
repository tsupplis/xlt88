# Translation difference audit

Identical normalized inputs, fresh XLT86 1.1, current xlt88, ASM86 1.1N2.
Text was compared after accounting for symbol names, numeric spelling, EQU
address aliases and separate data directives. Instruction bytes come from
listings, cross-checked against H86. This is not exhaustive equivalence proof.

| Program | XLT86 instruction bytes | xlt88 | Difference |
| --- | ---: | ---: | ---: |
| VTL-2 | 1626 | 1743 | +117 |
| WASH15, XLT86 explicit port | 1672 | 1965 | +293 |
| DISK7, both explicit ports | 3440 | 3937 | +497 |

Raw DISK7 is 3429 versus 3932; port changes must not be mistaken for optimizer
effects. Existing reference inputs remain unchanged. The benchmark reports now
include instruction-family byte accounting, not just mnemonic counts.

## VTL-2: classification

The 28 auxiliary-carry inversions follow these source control-flow paths:

| First observation or analysis boundary | Sites | Classification |
| --- | ---: | --- |
| Reachable PUSH PSW (some paths also return) | 11 | Flag value is materialized; retain pending proof that saved bits are unobserved |
| Return only | 14 | Conservative; caller-sensitive proof required before removal |
| Unresolved BDOS call (possibly also a return) | 3 | Conservative; needs a service contract |

Examples of PSW paths: evil, eval3, var1, inln2/inln6, mult2/div3 and scrnlf.
Return boundaries include cphd, cpbd, hsubb, tstn and scrcpy. A return is not
proof that auxiliary carry is used. Conversely, matching a console transcript
is not proof that it is dead. Unknown calls are not silently treated as kills.

The complete net instruction-byte difference can be accounted for as follows:

| Instruction family | xlt88 minus XLT86 bytes |
| --- | ---: |
| LAHF/SAHF | +33 |
| XOR (28 auxiliary-carry inversions) | +84 |
| AND/OR | +26 |
| PUSH/POP | +16 |
| XCHG | +6 |
| ADD/SHL | +4 |
| MOV | -52 |
| Other, net | 0 |
| Total | +117 |

These are families, not independent semantic causes. For example, AND's
8080 auxiliary-carry handling contributes to several rows. ASM86 inserts 68
CS overrides in XLT86's VTL memory accesses; xlt88 uses DS under the shared
segment model. Those 68 bytes saved offset additional MOV instructions.
Both load spans remain 1936 bytes: extra code consumes fixed-ORG padding.

XLT86 also omits some accumulator-restoring XCHG instructions after PUSH PSW.
xlt88 preserves A without general register liveness. These are candidates for
analysis, not established defects or permission to drop restores globally.
The observed SUI/PUSH PSW mismatch is documented in README.

One proven redundancy was removed generically: SAHF; word INC/DEC; SAHF can
omit the first SAHF. The step does not read the restored flags or change AH;
the last SAHF restores the same bits, and OF is unaffected by either SAHF.
Labels, AH/AX changes and other instructions stop this optimization. It saves
one additional byte in VTL's nxtuu path; WASH and DISK are unchanged.

## WASH15 and DISK7: additional differences

| Pattern | WASH15 | DISK7 | Interpretation |
| --- | ---: | ---: | --- |
| Auxiliary-carry inversions | 25 | 56 | Same conservative flag strategy as VTL; not all proved necessary |
| Static PUSH ES sites | 30 | 37 | Native-call preservation, including adapter/dispatch code |
| Reset block instruction bytes | 61 | 61 | XLT86 uses MOV CL,13; INT 224 (4 bytes); xlt88 additionally selects drive, scans and restores DMA |
| Reset data/reserved bytes added | 15/149 | 15/149 | OS semantics, not instruction optimizer overhead |
| Extra near-JMP bytes | 51 | 81 | Branch expansion/dispatch; investigate ranges before changing |

WASH's dynamic output selection has a 44-byte flag-preserving dispatcher for
services 2/5/4 plus fallback; XLT86 emits one 2-byte interrupt. Factoring common
native lowerings is a concrete future opportunity, but the fallback and flags
must keep their contracts. DISK's function-6 lowering also checks the CP/M-80
FEh output convention; native pass-through alone is not the same contract.

WASH PNAME accesses use address EQU aliases in xlt88; the XLT86 port uses DI
to avoid ASM86's code-label type rejection. The accesses remain byte-sized.
DISK still needs the five explicit ES pointer-read overrides in both ports.
The comparison does not resolve its CP/M 2.2/3.x allocation assumptions.

WASH interrupt state differs: the patched reference adds STI after stack
setup; xlt88 translates the original DI and leaves interrupts disabled.
This is an outstanding native-port review item, not grounds to invent an
automatic STI rule. Emulator transcript parity does not validate interrupt
delivery. DISK's reference likewise includes explicit CLI/STI stack edits.

No new numerical xlt88 defect was demonstrated by this audit. Unproved
redundancy remains unproved. Next priorities are return-flag contracts and
factoring equivalent native-call dispatch, followed by branch-range analysis.
Current validation: optimizer unit cases, eight flag-flow and branch variants,
VTL source/COM on eight target/emitter pairs, and four-emitter WASH/DISK
reference checks with documented ports. The full CPU matrix was not rerun.
