# Translation difference audit

Identical normalized inputs, fresh XLT86 1.1, current xlt88, ASM86 1.1N2.
Text was compared after accounting for symbol names, numeric spelling, EQU
address aliases and separate data directives. Instruction bytes come from
listings, cross-checked against H86. This is not exhaustive equivalence proof.

| Program | XLT86 instruction bytes | xlt88 | Difference |
| --- | ---: | ---: | ---: |
| VTL-2 | 1626 | 1743 | +117 |
| WASH15, XLT86 explicit port | 1672 | 1946 | +274 |
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
| Static PUSH ES sites | 28 | 37 | Native-call preservation, including adapter/dispatch code |
| Reset block instruction bytes | 61 | 61 | XLT86 uses MOV CL,13; INT 224 (4 bytes); xlt88 additionally selects drive, scans and restores DMA |
| Reset data/reserved bytes added | 15/149 | 15/149 | OS semantics, not instruction optimizer overhead |
| Extra near-JMP bytes before dispatch sharing | 51 | 81 | Branch expansion/dispatch; investigate ranges before changing |

WASH's dynamic output selection now has a 25-byte flag-preserving dispatcher
for services 2/5/4 plus fallback, reduced from 44 bytes; XLT86 emits one 2-byte
interrupt. The backend declares equivalent native lowerings. Proven sets use
one body directly; guarded sets retain every comparison and unmatched native
fallback. Flags are restored on both matched and unmatched paths. Reset,
direct I/O, far pointers and unsupported services are excluded. WASH saves
19 code bytes and 16 loaded bytes (3904 to 3888); VTL/DISK are unchanged.
DISK's function-6 lowering also checks the CP/M-80
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

## Executed sequence classification

`tests/xltseq.py` compares native excerpts with spelling normalized to NASM,
separately from fresh source probes whose observation can change optimization.
Each passing native variant completes 128 states: all 32 source PSWs and
four word boundaries. Checks capture flags before comparisons and use byte
canaries around odd-address memory. Negative witnesses stop at first mismatch.

| Difference / location | Verdict and evidence |
| --- | --- |
| Short branch versus inverted condition + JMP | Equivalent in the tested branch/PSW states; existing branch matrix also passes |
| Repeated versus shared flag saves around word INC | Equivalent in all tested states |
| SHL versus ADD doubling inside saved PSW | Equivalent for tested results, both carry paths and restored PSW |
| Omitted A restore after PUSH PSW: mult1/div1/scrnlf | Valid in inspected contexts: mult1 restores before A use; div1 overwrites A in hsubb; scrnlf overwrites it with LDA |
| Same omitted restore with A immediately observed | Invalid as a general substitution; native witness fails. Fresh XLT86 source probe with A live passes, confirming context-sensitive selection |
| Omitted SAHF after POP PSW in term | Old flags are overwritten by CPI before observation; tested overwrite pattern is equivalent |
| SI versus DI byte access; direct odd word access | Tested values/widths and neighbour bytes preserved; no general permission to widen byte accesses |
| CS versus DS access | Equivalent under shared segments. Deliberately different segments read 22h versus 77h: explicit model assumption |
| Auxiliary-carry repairs | Six full-PSW witnesses below support xlt88; XLT86 differs. Whether every omitted repair affects application behavior remains separate |
| BDOS 13 adapter in WASH/DISK | Fresh XLT86 probe fails reset-to-A; xlt88 passes. Existing reset suite also verifies DMA, dollar-file result and file preservation |
| BDOS 6 FEh adaptation in DISK | Fresh XLT86 emits no FEh character; xlt88 emits FEh under the emulator |
| WASH PNAME addressing | Both patched DI access and xlt88 EQU access are byte-sized; selected memory-width tests and WASH scenarios pass |
| DISK five ES reads | Explicit ports required; native far-pointer suite passes. No automatic pointer conversion claimed |
| WASH STI/manual stack edits | Observable port change, not equivalent to source DI. Hardware interrupt behavior remains unverified |

Fresh source witnesses (hex A/PSW; expected from the existing 8080 model):

| Operation / inputs | Expected = xlt88 | XLT86 |
| --- | --- | --- |
| SUI 1, A=10, carry=0 | 0F/06 | 0F/16 |
| CPI 1, A=10, carry=0 | 10/06 | 10/16 |
| SBB B, A=10, B=01, carry=1 | 0E/02 | 0E/12 |
| DCR A, A=10, carry=0 | 0F/06 | 0F/16 |
| ANA A, A=08 | 08/12 | 08/02 |
| ANI 7F, A=00 | 00/56 | 00/46 |

These invalidate full-PSW equivalence for those XLT86 source probes, not the
working application transcripts. OS observations use the configured emulator;
device behavior and real interrupt delivery remain outside that evidence.
No xlt88 defect was exposed by these checks. This classifies selected differing
patterns, not every possible application execution or register substitution.

## Caller-aware flag analysis

Following only named calls would be unsound: cpbd has two explicit callers
that use Z, but also receives fall-through from the preceding expression
routine. Likewise cphd is entered through lxhn. tstn can return flags through
cvtln/cvbin's conditional returns. PSW saves may transport flag bits on the
stack rather than inspect them immediately. None of these are special-case
optimization permissions; no return-flag corrections were removed.

A general extension needs control-flow entry/return ownership, including
fall-through and tail transfers, plus conservative treatment of escaped code
addresses and altered return stacks. flow.c now supplies the graph consumed
by flag analysis: fall-through crosses labels, direct targets and call
continuations are distinct, and conditional returns retain their skipped path.
Indirect targets, ambiguous/data labels and return destinations stay unknown.
The old private edge resolver is removed. A bounded call-site proof now
summarizes reads/preserved flags along stack-neutral, store-free leaf paths.
It uses the particular caller's continuation; conditional returns and direct
tail branches are included. Nested calls, stack changes, memory writes and
unresolved paths fall back conservatively. Other entries do not invalidate
this invocation's proof, because shared callee code is not weakened.
The regression includes a direct caller that discards flags and a second
caller falling through into the same routine that observes PSW. Both must work.
A focused two-version assembly comparison shrinks a caller/leaf example from
40 to 35 instruction bytes. VTL/WASH/DISK gain no further bytes from this
bounded proof. Global removal of shared return flags remains unproved; do not
expand this into a general return-stack analysis merely to chase size parity.

No new numerical xlt88 defect was demonstrated by this audit. Unproved
redundancy remains unproved.
Current validation: optimizer unit cases, eight flag-flow and branch variants,
VTL source/COM on eight target/emitter pairs, and four-emitter WASH/DISK
reference checks with documented ports. The full CPU matrix was not rerun.
Dispatch sharing additionally passes backend equivalence checks against every
eligible individual service, eight-target service-set and guarded-selector
execution (including alias mutation and singleton fallback), and native-call
contract tests. No DOS lowering was changed.
