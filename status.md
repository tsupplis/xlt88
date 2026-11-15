# Feature status

CLOSED applies to the documented contract; reopen for defects or agreed scope.

| Feature | Status / boundary |
| --- | --- |
| Parser → translator → emitter | CLOSED modular interfaces |
| ASM80 / M80 / MAC | CLOSED source subsets; inputs.md |
| RMAC | CLOSED single-module contract; no REL linking |
| 8080 instructions | CLOSED native translation; RST 0 exits, RST 1–7 require monitor ports |
| NASM / ASM86 / MASM 3 / DRASM86 | CLOSED, both targets |
| Normalized 8080 output / text formats | CLOSED; macro expansion, names and round trips |
| Labels / EQU naming state | CLOSED for unchanged source; labels.md |
| CP/M-86 native BDOS lowerings | CLOSED, 39/39 entries; api.md lists runtime/port exceptions |
| DOS BDOS mapping | PARTIAL, 25/39; directory search and broader FCB semantics remain |
| COM input | PARTIAL: VTL-2 reviewed COM/source execution matches all eight combinations; automatic native recovery remains open |
| State rebinding / assembler-map import | DEFERRED |
| REL linking / CMD allocation policy | DEFERRED |

Translation assumes static code; generated/self-modifying instructions need an explicit port.
Internal representation cleanup is maintenance, not an open functional feature.
IN/OUT/DI/EI/HLT have native instruction checks, not hardware-equivalence proof.

## Evidence and dependency

Full closure regression passed: 78 mnemonic fixtures, 70,400 arithmetic,
8,192 AND and 81,536 additional CPU cases across eight target/emitter pairs.
The extra CPU matrix used a temporary emulator AF correction for
[emu2 #94](https://github.com/johnsonjh/emu2-cpm86/issues/94).
Until installed emu2 is corrected, XLT_CPU_EMU can select a corrected binary
for cpufinal.py alone. No generated-code workaround or masked test failure.
COM was added after that full run began and passed separate opcode, map/state,
boundary, eight native execution, sanitizer and static-analysis checks.
Housekeeping then passed existing normalized-emitter/state/interface tests.
The COM readability/indirect-word batch passed eight native pointer-relocation
paths, VTL source/COM console and script checks,
ASan/UBSan, and four-emitter WASH/DISK7 reference checks. DISK7 uses the explicit
five-read ES test port; this batch did not rerun the complete CPU matrix.

| Application | Tested acceptance / remaining boundary |
| --- | --- |
| VTL-2 | Source and reviewed COM console/script execution agree, both targets/all emitters; ORG 0800h retained |
| WASH15 | Twelve CP/M-86 reference scenarios; explicit-port copies pass. DOS/original allocation assumptions remain |
| DISK7 | Explicit five-read ES port: CP/M-86 reference browsing/free-space/copy checks; allocation and 3.x remain open |
| DRI filename parser | Unchanged RMAC source passes tested scenarios, all eight combinations |
| Forth | Explicit console-only port passes; disk/printer work remains |
| PILOT / Tiny BASIC / Balloon | Monitor/restart/layout/FCB or hardware port blockers |

References in examples/ref are preserved: vtl2.a86 is direct xlt86 output;
was15.a86 and disk7.a86 include manual SS/interrupt/label-access/far-pointer
patches. Those patches are not generic translator rules. WASH reduced-memory
tests simulate base-page limits with canaries, not actual CMD allocation.

Next: measured flag/call-return analysis, DOS search/FCB semantics, and
DISK7 allocation/3.x port boundaries. Assembly success is not runtime equivalence.

Fresh XLT86 comparisons now use identical prepared inputs for VTL-2, WASH15 and
DISK7. Instruction/data/reserve/ORG counts and explicit port edits are reported
separately; existing application checks pass. Full comparative register, memory
access and flag equivalence remains unverified. See README for measured results.
The flag-image reuse optimization passed 70,400 arithmetic and 8,192 AND
result/PSW cases, branch/flag-flow checks across all eight combinations, and
VTL/WASH/DISK reference checks (DISK retains its explicit ES port). The separate
81,536-case CPU matrix was not rerun in this batch. A focused fresh-XLT86 probe
also records an auxiliary-carry difference after SUI/PUSH PSW; see README.
The subsequent textual audit accounts for VTL's 117-byte gap, classifies its
28 AC corrections and inventories WASH/DISK differences; see compare.md.
One redundant SAHF was removed; focused flag/branch and application checks pass.
WASH's disabled interrupts remain an explicit native-port review item.
