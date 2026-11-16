# xlt88

Native 8080-to-8086/8088 source translation for CP/M-86 1.1 and PC DOS 1.0.
No CP/M emulator or universal compatibility runtime.

Translation assumes **static code**. Self-modifying code, generated instructions
and runtime patching of instruction operands require explicit native ports.
Writable data and pointer tables are different: their addresses and uses must
still be understood and relocated correctly.

## Use

```sh
make
./xlt88 --target=msdos --dialect=asm80 source.asm dos.asm
nasm -f bin dos.asm -o dos.com
./xlt88 --target=cpm86 --emit=asm86 source.asm cpm.a86
cpm86_asm86 cpm
cpm86_gencmd cpm 8080
```

Defaults: CP/M ASM (asm80) input and DOS target. Output `.a86` selects ASM86; `.asm` and
other extensions select NASM. `--emit` overrides that choice. Input dialect
and OS are never inferred from extensions. Use `./xlt88 --help` for all options.
Readers: asm80, m80, mac, rmac, com. Emitters: nasm, asm86, masm3, drasm86, asm80.
For NASM CP/M output, assemble a flat binary then use bin2cmd to create a CMD.
MASM 3 and RASM-86 use the tested Microsoft LINK/EXE2BIN packaging path:

```sh
./xlt88 --emit=masm3 source.asm output.asm
pcdev_masm3 'output.asm,output.obj;'
pcdev_link2 'output.obj,output.exe;'
pcdev_exe2bin output.exe output.com
```

For drasm86, generate output.a86 and run `pcdev_rasm86 output`, then LINK/EXE2BIN.
CP/M packaging uses `bin2cmd output.com output.cmd`. Tested assemblers: ASM86
1.1N2, MASM 3.00, RASM-86 1.4a; LINK 2.40. The no-STACK linker warning is expected.
Legacy output uses CRLF + final Ctrl-Z; NASM uses LF. Fixed-origin expansion
must fit. Legacy name collisions/invalid expressions fail before replacement.

## Intermediate ASM80 (CP/M ASM)

`--emit=asm80` emits normalized CP/M ASM source before CPU/OS lowering, with macro
expansion and preferred names. Read it back using `--dialect=asm80`.

```sh
./xlt88 --dialect=m80 --emit=asm80 source.m80 review.asm
./xlt88 --dialect=mac --emit=asm80 source.mac review.asm
./xlt88 --dialect=rmac --emit=asm80 source.asm review.asm
./xlt88 --dialect=com --com-map=input.map --emit=asm80 input.com review.asm
```

## COM input

```sh
./xlt88 --dialect=com --emit=asm80 input.com recovered.asm
# Review recovered.asm and recovered.map; resolve unknown regions and pointers.
./xlt88 --dialect=com --com-map=recovered.map --target=cpm86 input.com output.a86
```

COM discovery feeds the normal selected emitter: one requested assembly output,
plus the analysis map. Automatic COM recovery is **experimental**.
Unreviewed recovery can be emitted only as ASM80 for inspection (exit **2**);
Native emission is blocked (exit **1**) until a reviewed map is supplied.
Exit 0 means reviewed-map/text translation succeeded; 1 means an error.
Reviewed VTL-2 COM input now matches source-input execution on both targets and
all four emitters. Complex COM files still need substantial review. See [COM limitations](com.md).

## xlt88 versus XLT86

XLT86 is stronger in optimization controls and memory-model choices. xlt88 is
stronger in integrated macro reading, DOS targeting and assembler choice.
“Not provided” is different from “implemented but not yet usable”.

| Area | XLT86 | xlt88 | Advantage / consequence |
| --- | --- | --- | --- |
| Optimization analysis | Global register/flag liveness | Flag liveness, branch shortening and targeted OS/FCB analyses; no equivalent general register-liveness optimizer | **XLT86: smaller instruction streams in the three measured cases**; semantic and speed superiority are not established |
| Optimization controls and inspection | Block traces and return-flag assumptions | No comparable user-facing trace or return-contract controls | **XLT86** for inspecting/tuning translation |
| Memory models | Overlapping 8080 model, compact model and segment controls | Shared 8080-model layout; RMAC sections flattened; no compact-model output | **XLT86** for separate code/data layouts |
| Macro programs | Requires externally expanded macro/repeat source | Built-in M80/MAC/RMAC expansion within documented subsets | **xlt88**: fewer preparation steps; unsupported dialect semantics must fail |
| DOS targeting | No DOS backend documented | 25/39 source BDOS functions mapped to DOS, including FCB adapters | **xlt88**: a capability XLT86 does not provide; incomplete coverage remains |
| Assembler choice | ASM86 output | NASM, ASM86, MASM 3, DRASM86 | **xlt88**: four usable toolchains |
| Editable naming state | No comparable round-trip state documented | Export/import preferred labels and EQU names for unchanged source | **xlt88** for repeatable renaming |
| COM binaries | No binary-input reader documented | Automatic draft map/listing, then reviewed-map translation; difficult binaries remain unresolved | **xlt88 has a facility, not a closed solution**; native output requires a reviewed map |
| CP/M-86 correctness | Native translation, with manual patches needed in two supplied ports | 39/39 documented lowerings, with native-structure/segment and application assumptions still requiring review | **No demonstrated general winner**; function counts do not prove semantic equivalence |
| CPU correctness | Reference application behavior available | Extensive instruction/flag regression, with an identified emulator dependency | **No demonstrated winner** without running the same CPU suite against both |
| Application readiness | Working VTL-2 directly; working WASH15/DISK7 after patches | VTL-2 passes both targets/four assemblers; WASH has port boundaries; DISK7 explicit ES port matches browsing and copy checks | **Tested scenario parity**, with manual port boundaries remaining |

XLT86 capabilities above come from its [1981 User's Guide](https://bitsavers.trailing-edge.com/pdf/digitalResearch/cpm/XLT86_Users_Guide_Sep81.pdf),
sections 1–3; xlt88 claims come from current code and recorded tests. “Not
documented” is limited to that version. VTL-2 checks expected result tokens,
not exhaustive equivalence.

Fresh XLT86 1.1 measurements use identical prepared ASM80 inputs, the same
ASM86 assembler and GENCMD 8080 model. The supplied patched references remain
separate runtime oracles. Counts below are instruction bytes from assembler
listings, cross-checked with initialized H86 byte totals; no timing claim is made.

| Program | Fresh XLT86 instruction bytes | xlt88 instruction bytes | Runtime comparison / adjustments |
| --- | ---: | ---: | --- |
| VTL-2 | 1,626 | 1,743 | Console and script transcripts match; no reference port edits |
| WASH15 | 1,672 | 1,946 | Five scenarios match after XLT86 SS/interrupt and PNAME-access fixes; raw output has two assembly errors |
| DISK7 | 3,440 | 3,937 | Browsing, free-space and copying match; XLT86 SS setup and five ES reads, xlt88 five ES reads |

VTL-2 has 129 versus 12 bytes of ORG padding: both loaded spans round to 1,936
bytes despite the instruction difference. WASH15 and DISK7 each add 15 initialized
and 149 reserved bytes in xlt88's BDOS reset adapter; this implements source OS
behaviour and must not be counted as gratuitous optimizer overhead. Reports
separate instruction/data/reserve/ORG bytes and rounded load spans. DISK7's raw
instruction counts are 3,429 / 3,932 before explicit target-port edits.

DISK7 preparation expands MAC through the normalized emitter, renames seven
symbols, restores the conditional CLRSCR code label, defers RINGBASE EQU, removes
END's operand and splits DB strings. Both translators receive those same bytes.
Reports record common-input/raw-XLT86 hashes and all explicit target adjustments.

The first optimization removes a redundant LAHF when AH already contains the
canonical flag image restored by SAHF. It saves one instruction byte in VTL-2
and two in DISK7; WASH15 is unchanged. No memory access or register width changes.
A subsequent redundant SAHF removal saves one more VTL byte. The
[textual difference audit](compare.md) accounts for the net byte gap and
classifies flag, register, OS-adapter and native-port differences in all three.
Sharing equivalent guarded CP/M-86 service bodies saves 19 WASH bytes while
retaining the original selector checks and native fallback.

Optimization priorities from the listings:
1. Investigate flag preservation first: VTL-2 has 32/32 LAHF/SAHF instructions
   from XLT86 versus 48/49 from xlt88. Remove saves only when unobservability is proved.
2. Separate BDOS adapter and register-save costs from general register allocation;
   WASH15/DISK7 differences include deliberately stronger OS lowering.
3. Evaluate register substitutions only with explicit alias and byte-access contracts.

These application comparisons **do not establish full register, memory or flag
equivalence**. In particular, effects on both register halves, widened reads/writes,
neighbouring bytes, odd addresses, fixed alignment and every observable 8080 flag
(including auxiliary carry and PUSH PSW/DAA uses) remain constraints on optimization.
Self-modifying/runtime-generated code is outside the automatic translation contract.
A focused benchmark probe now checks `A=10h; SUI 1; PUSH PSW`: fresh XLT86
1.1 produces accumulator/PSW `0F/16`, while the existing 8080 reference model
expects `0F/06` and xlt88 matches it. This exposes an auxiliary-carry difference
in that case; it is not a full comparative CPU validation. No optimization has
been justified merely by XLT86 producing fewer bytes.
The [executed sequence classification](compare.md) now adds six full-PSW
witnesses, native register/memory/flag comparisons and fresh BDOS 6/13 probes.
Run `tests/xltseq.py` with an optional report path; it uses the same XLT86 override.

Reproduce using `tests/vtlbench.py`, `tests/wash.py` and `tests/diskref.py`, each
with an optional report path. Their shared `tests/xltref.py` runs fresh translation
through `cpm_xlt86` (`XLT86` overrides the wrapper path).

## Tests and documentation

`make test` runs regression; `make corpus` inventories examples; `make vtlbench`
runs the VTL-2 comparison. Python 3 and the assembler/emulator tools above plus
emu2 and bin2cmd must be on PATH. Overrides are in tests/toolrun.py.
Normalized-source/VTL checks also use `cpm_asm80` (`ASM80` override);
application comparisons require `cpm_xlt86` (`XLT86` override).
Tests print results; a report-producing test accepts an optional output path,
for example `python3 tests/vtlbench.py /tmp/vtlbench.md`. Normal tests do not
recreate documentation. Keep repository Markdown within 800 lines total.

[Status and evidence](status.md) · [Architecture](design.md) ·
[Source dialects](inputs.md) · [Annotated COM](com.md) ·
[Labels/EQU state](labels.md) · [OS contracts](api.md).
