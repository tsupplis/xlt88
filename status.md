# xlt88 global status — 14 November 2026

The project is a native translator, with no CP/M machine emulation or universal
BDOS compatibility runtime. The native CP/M-86 entry set is now complete;
automatic translation of arbitrary programs and full behavioral validation are
not complete. CMD allocation policy remains deferred for a separate design review.

## Implementation and evidence

| Area | Current position | Remaining work |
| --- | --- | --- |
| Architecture | Separate readers, shared symbols/layout/analysis, CPU and OS lowering, owned target program and assembler emitters; injected interfaces | Finish typed source operands and structured memory operands; preserve modularity as analysis grows |
| Inputs | Intel implemented; M80/MAC partial readers; RMAC single-module placement | RMAC module linking and broader dialect coverage; annotated COM decoding |
| Outputs | NASM, ASM86, MASM 3 and RASM-86 assemble and execute regression fixtures on both targets | Broader program/layout coverage; segmented target representation |
| Instruction translation | 78/78 mnemonic fixtures; 70,400 arithmetic/result/PSW cases; 8,192 AND/PSW cases | Exhaustive instruction/operand/flag coverage matrix and untested boundaries; hardware-dependent instructions |
| CP/M-86 BDOS | All 39 non-reserved CP/M 2.2 native entry points emit and assemble through all four emitters | 27/31 consumers require explicit segment-aware port review. Device, IOBYTE, user/protection state and zero-fill execution limits remain; see api.md |
| DOS BDOS mapping | 25/39 entries (buffered input has an explicit capacity limit); nine use validated FCB plans | Capacity-255/native console-policy differences, directory-result conversion, user/device/attribute/disk-policy differences and unsupported calls; native DOS-only API inventory |
| Symbols and placement | Editable name/EQU state, source identities, fixed-origin planning and branch-size optimization | State rebinding after source edits, explicit alignment contracts, segment/provenance analysis and assembler-map feedback |
| Generic recognition | Bounded register facts and guarded DS 1 memory-loaded selectors | Broader control-flow/call effects; CP/M-86 unmatched values pass through natively with warnings, DOS exits. No general alias proof |

The CP/M-86 count is native entry coverage. For 27/31 the emitted call is tested,
but source dereferences are not automatically converted to use ES. They emit
both a diagnostic and a target comment requiring review. Do not read 39/39 as
100% automatic CP/M-80 semantics or 100% tested behavior.

## Application comparison

| Program | Working reference | xlt88 evidence | Main gap |
| --- | --- | --- | --- |
| VTL-2 | Direct xlt86 reference | Interactive and script-file comparisons pass on both targets, all four emitters | More language/input boundaries and explicit native-memory assumptions |
| WASH15 | xlt86 plus supplied manual target patches | Five console scenarios plus seven file-operation/cancellation scenarios match on CP/M-86, all four emitters; source/reference unchanged | Copy is layout-sensitive: reference/NASM time out in probes; three other emitters complete. DOS FCB representation, device/drive and interrupt-policy work remain |
| DISK7 | xlt86 plus supplied preprocessing and target patches | Four reference-only scenarios pass; unchanged source translates and assembles with all four CP/M-86 emitters | Native 27/31 consumers, BIOS/memory/exit assumptions need port review; generated code not executed |
| Hello / CPU diagnostic | Independent expected outputs | Both targets, all four emitters execute | Broaden instruction coverage independently |
| Forth | Explicit console-only source port | Console/arithmetic/definition/exit acceptance on both targets | Original disk/printer/BIOS dependencies |
| PILOT / Tiny BASIC / Balloon | No passing whole-program acceptance in this batch | Parsing/translation blockers recorded in corpus.md | Monitor hooks, restart bodies/layout/FCBs or machine-specific hardware, respectively |

WASH CMD sizes (container and padding included): reference 3,840 bytes;
ASM86 output 4,352 bytes (+13.3%); NASM/MASM3/RASM-86 output 4,608 bytes (+20%).
These are footprint measurements, not instruction-count or equivalence claims.
DISK7's reference is 5,248 bytes; generated assembly success is not execution equivalence.

## Regression approach

Use both layers:

1. Independent CPU, service-contract, parser, symbol, layout and emitter tests
   establish generic rules and exercise positive, negative and boundary cases.
2. Working-reference comparisons establish application behavior on the paths
   exercised: full transcripts and file contents from identical initial fixtures.

Assembly inspection explains differences in segments, stacks, interrupt policy,
layout and size. Exact assembly matching is not an acceptance criterion.
The reference patches remain explicit port decisions; they are not rules keyed
to program names. The far-pointer ABI tests deliberately use test-only manual
consumer adjustments and are labelled accordingly.

## Current batch and next decisions

The native pass-through policy is implemented for CP/M-86, with source and
assembly warnings. Known calls retain their adaptations; unknown calls have
native contracts. DOS remains strict. Native extension pass-through does not increase the
CP/M 2.2 coverage count; DOS reached 25/39 with the buffered-input adapter.

WASH has seven additional exact transcript/file comparisons. Its copy-completion
probes expose limits in the original allocation assumptions; see washfile.md.
DOS blockers have been analyzed without loosening the FCB audit; see washdos.md.
DISK7 was treated as the agreed diagnostic checkpoint, not a whole-program port.

The explicit test-only WASH allocation port now passes 18 copy/replace cases
per emitter, plus directory-overflow rejection with files unchanged. Small
allocation limits are simulated base-page descriptors with canary checks;
these are not actual CMD allocation tests. See washport.md. The original
source/reference comparisons remain separate and unchanged.

ASM86's loop was an expression-evaluator stack overflow in the assembler.
Increasing its private stack from 600 to 2048 bytes resolves DISK7 assembly.
All non-NASM emitters now write CRLF and final Ctrl-Z; NASM retains LF.

Current functional batch implements DOS BDOS 10 for capacities 0..254,
generic split-label FCB/DMA declarations, and RMAC single-module source
placement. Focused tests pass across all four emitters, including the unchanged DRI
filename parser. Full regression and sanitizer validation have passed.

Next work:

1. Broaden RMAC with real relocatable modules and a deliberate linking contract;
   EXTRN/COMMON and mixed placement are still unsupported.
2. DOS directory-result conversion and bounded FCB subroutine effects.
3. DISK7 manual segment/BIOS review; annotated COM decoding remains planned.

CMD sizing/allocation policy remains deferred for a separate design discussion.

Implicit alignment inference remains later work. First record explicit placement
constraints and diagnostics, following porting.md. There is no reason to force
hardware, segment or algorithmic policy choices into speculative heuristics.

Detailed evidence: api.md, progress.md, wash.md, diskref.md, vtlbench.md and
corpus.md. Porting guidance: porting.md.


Prior batch validation: the full make test gate passed, including the
70,400 arithmetic and 8,192 AND/PSW cases, native far-result tests, VTL-2 on both
targets, WASH transcript comparisons and DISK7 reference-only acceptance.
The additional full-entry assembly fixture passed separately on all four
emitters and is included in future make test runs. Strict C99 compilation,
Clang analysis and focused ASan/UBSan far-result/service-dispatch tests passed.
The corpus report was regenerated. Original program and reference files were
not modified. Current-batch validation is recorded below.


Previous batch validation: make test completed successfully, including 70,400
arithmetic/result/PSW and 8,192 AND/PSW cases, VTL-2 on both targets, all twelve
WASH comparison scenarios, and the DISK7 diagnostic checkpoint. The latter
records an ASM86 timeout rather than claiming assembly success. WASH copy
probes record reference/NASM timeouts; ASM86/MASM3/DRASM86 additionally pass
513-record copies with exact contents and source preservation.

Strict C99 compilation, Clang analysis and focused ASan/UBSan native-call and
selector tests passed. The final extension-continuation tests passed without
assuming that emu2 rejects function 46. Wrapper timeouts now stop the spawned
process group; a focused timeout check and the final checkpoint exercised it.
The corpus and status reports are refreshed. Original examples and references
remain unchanged. This historical validation predates the ASM86 repair and
the WASH explicit-port checks above.

Latest validation: the full `make test` gate passed after the assembler repair
and text-format fixes. All four DISK7 assembly checks passed. The new WASH
explicit-port test passed separately on all four emitters and is registered
in `make test` for subsequent runs. The refreshed corpus records DISK7 at
4/8 translate and 4/8 assemble combinations (all four CP/M-86 emitters; DOS
remains blocked). Byte-format and far-pointer tests also passed after the
test harness format correction. No original examples or references changed.

RMAC/DOS batch validation: full `make test` completed successfully, including
70,400 arithmetic/result/PSW cases, 8,192 AND/PSW cases, service/file tests,
VTL-2 interactive/scripts, WASH comparisons/explicit-port tests and all four
DISK7 assembly checkpoints. Final RMAC legacy-source/lexical checks passed
under ASan/UBSan; storage and buffered-input focused sanitizer runs passed.
Strict C99 warnings-as-errors compilation and Clang analysis of the new
modules passed. The shared reader mode cleanup was followed by M80 macro and
MAC regression checks. New source fixture `tests/parse.asm` is byte-identical
to the local DRI CP/M 3 parser; original examples/references remain unchanged.
