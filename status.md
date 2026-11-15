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
| COM input | OPEN: automatic draft map/listing and reviewed-map translation; LADCONF recovery incomplete |
| State rebinding / assembler-map import | DEFERRED |
| REL linking / CMD allocation policy | DEFERRED |

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

| Application | Tested acceptance / remaining boundary |
| --- | --- |
| VTL-2 | Interactive/script scenarios pass, both targets/all emitters; ORG 0800h retained |
| WASH15 | Twelve CP/M-86 reference scenarios; explicit-port copies pass. DOS/original allocation assumptions remain |
| DISK7 | All CP/M-86 emitters assemble; generated runtime acceptance open |
| DRI filename parser | Unchanged RMAC source passes tested scenarios, all eight combinations |
| Forth | Explicit console-only port passes; disk/printer work remains |
| PILOT / Tiny BASIC / Balloon | Monitor/restart/layout/FCB or hardware port blockers |

References in examples/ref are preserved: vtl2.a86 is direct xlt86 output;
was15.a86 and disk7.a86 include manual SS/interrupt/label-access/far-pointer
patches. Those patches are not generic translator rules. WASH reduced-memory
tests simulate base-page limits with canaries, not actual CMD allocation.

Next: one real COM program through discovery and reviewed translation, DOS search/FCB semantics, then explicit
DISK7 runtime comparison. Assembly success is not runtime equivalence.
