# xlt88 global status — 14 November 2026

The project is a native translator, with no CP/M machine emulation or universal
BDOS compatibility runtime. The native CP/M-86 entry set is now complete;
automatic translation of arbitrary programs and full behavioral validation are
not complete. This snapshot is a pause point before choosing more work.

## Implementation and evidence

| Area | Current position | Remaining work |
| --- | --- | --- |
| Architecture | Separate readers, shared symbols/layout/analysis, CPU and OS lowering, owned target program and assembler emitters; injected interfaces | Finish typed source operands and structured memory operands; preserve modularity as analysis grows |
| Inputs | Intel implemented; M80 and MAC useful partial readers; unchanged DISK7 parses | Remaining dialect semantics and relocation; RMAC; annotated COM decoding |
| Outputs | NASM, ASM86, MASM 3 and RASM-86 assemble and execute regression fixtures on both targets | Broader program/layout coverage; segmented target representation |
| Instruction translation | 78/78 mnemonic fixtures; 70,400 arithmetic/result/PSW cases; 8,192 AND/PSW cases | Exhaustive instruction/operand/flag coverage matrix and untested boundaries; hardware-dependent instructions |
| CP/M-86 BDOS | All 39 non-reserved CP/M 2.2 native entry points emit and assemble through all four emitters | 27/31 consumers require explicit segment-aware port review. Device, IOBYTE, user/protection state and zero-fill execution limits remain; see api.md |
| DOS BDOS mapping | 24/39 entries; nine use validated FCB plans | Buffered input, directory-result conversion, user/device/attribute/disk-policy differences and unsupported calls; native DOS-only API inventory |
| Symbols and placement | Editable name/EQU state, source identities, fixed-origin planning and branch-size optimization | State rebinding after source edits, explicit alignment contracts, segment/provenance analysis and assembler-map feedback |
| Generic recognition | Bounded register facts and guarded DS 1 memory-loaded selectors | Broader control-flow/call effects; unmatched guarded values terminate as diagnosed. No general alias proof |

The CP/M-86 count is native entry coverage. For 27/31 the emitted call is tested,
but source dereferences are not automatically converted to use ES. They emit
both a diagnostic and a target comment requiring review. Do not read 39/39 as
100% automatic CP/M-80 semantics or 100% tested behavior.

## Application comparison

| Program | Working reference | xlt88 evidence | Main gap |
| --- | --- | --- | --- |
| VTL-2 | Direct xlt86 reference | Interactive and script-file comparisons pass on both targets, all four emitters | More language/input boundaries and explicit native-memory assumptions |
| WASH15 | xlt86 plus supplied manual target patches | Five complete console transcripts match on CP/M-86, all four emitters; source and reference unchanged | File mutation/device/drive workflows; DOS FCB representation; real-hardware interrupt policy |
| DISK7 | xlt86 plus supplied preprocessing and target patches | Reference-only exit, browse, length and view pass twice; original MAC input parses | Two unresolved dynamic selectors, CP/M 3 function 46, explicit 27/31 segment-aware consumers; no generated executable comparison yet |
| Hello / CPU diagnostic | Independent expected outputs | Both targets, all four emitters execute | Broaden instruction coverage independently |
| Forth | Explicit console-only source port | Console/arithmetic/definition/exit acceptance on both targets | Original disk/printer/BIOS dependencies |
| PILOT / Tiny BASIC / Balloon | No passing whole-program acceptance in this batch | Parsing/translation blockers recorded in corpus.md | Monitor hooks, restart bodies/layout/FCBs or machine-specific hardware, respectively |

WASH CMD sizes (container and padding included): reference 3,840 bytes;
ASM86 output 4,352 bytes (+13.3%); NASM/MASM3/RASM-86 output 4,608 bytes (+20%).
These are footprint measurements, not instruction-count or equivalence claims.
DISK7's reference is 5,248 bytes; there is no comparable xlt88 binary yet.

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

## Proposed next batch — not started

1. Extend WASH reference comparison to copy, rename, delete, cancellation and
   failure paths, comparing file contents as well as console output.
2. Audit DISK7's dynamic selectors and version-dependent function 46 path.
   Decide which facts support a generic rule and which require an explicit
   CP/M-86 port. Keep its segment edits visible and reviewable.
3. Triage WASH DOS FCB and search-result assumptions before implementing more
   conversions. Do not assume native DOS directory results are CP/M entries.

Implicit alignment inference remains later work. First record explicit placement
constraints and diagnostics, following porting.md. There is no reason to force
hardware, segment or algorithmic policy choices into speculative heuristics.

Detailed evidence: api.md, progress.md, wash.md, diskref.md, vtlbench.md and
corpus.md. Porting guidance: porting.md.
