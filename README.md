# xlt88

Native 8080-to-8086/8088 source translation for CP/M-86 1.1 and PC DOS 1.0.
No CP/M emulator or universal compatibility runtime.

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
plus the analysis map. There is no additional `.80` file. Without a reviewed
map, successful emission returns **2 (draft, review required)**, including native
output. Exit 0 means reviewed-map/text translation succeeded; 1 means an error.
Complex COM files still need substantial review. See [COM limitations](com.md).

## xlt88 versus XLT86

XLT86 is stronger in optimization controls and memory-model choices. xlt88 is
stronger in integrated macro reading, DOS targeting and assembler choice.
“Not provided” is different from “implemented but not yet usable”.

| Area | XLT86 | xlt88 | Advantage / consequence |
| --- | --- | --- | --- |
| Optimization analysis | Global register/flag liveness | Flag liveness, branch shortening and targeted OS/FCB analyses; no equivalent general register-liveness optimizer | **XLT86: broader optimization machinery**; output size/speed superiority still needs measurement |
| Optimization controls and inspection | Block traces and return-flag assumptions | No comparable user-facing trace or return-contract controls | **XLT86** for inspecting/tuning translation |
| Memory models | Overlapping 8080 model, compact model and segment controls | Shared 8080-model layout; RMAC sections flattened; no compact-model output | **XLT86** for separate code/data layouts |
| Macro programs | Requires externally expanded macro/repeat source | Built-in M80/MAC/RMAC expansion within documented subsets | **xlt88**: fewer preparation steps; unsupported dialect semantics must fail |
| DOS targeting | No DOS backend documented | 25/39 source BDOS functions mapped to DOS, including FCB adapters | **xlt88**: a capability XLT86 does not provide; incomplete coverage remains |
| Assembler choice | ASM86 output | NASM, ASM86, MASM 3, DRASM86 | **xlt88**: four usable toolchains |
| Editable naming state | No comparable round-trip state documented | Export/import preferred labels and EQU names for unchanged source | **xlt88** for repeatable renaming |
| Intermediate 8080 view | Pseudo-assembly listing | Re-readable normalized source after macro expansion and renaming | **xlt88** for feeding reviewed source back into translation |
| COM binaries | No binary-input reader documented | Automatic draft map/listing, then reviewed-map translation; difficult binaries remain unresolved | **xlt88 has a facility, not a closed solution**; LADCONF is not yet translated |
| CP/M-86 correctness | Native translation, with manual patches needed in two supplied ports | 39/39 documented lowerings, with native-structure/segment and application assumptions still requiring review | **No demonstrated general winner**; function counts do not prove semantic equivalence |
| CPU correctness | Reference application behavior available | Extensive instruction/flag regression, with an identified emulator dependency | **No demonstrated winner** without running the same CPU suite against both |
| Application readiness | Working VTL-2 directly; working WASH15/DISK7 after patches | VTL-2 passes both targets/four assemblers; WASH has port boundaries; DISK7 generated runtime unvalidated | **XLT86-based ports currently ahead on DISK7**; VTL-2 shows practical parity on tested scenarios |

XLT86 capabilities above come from its [1981 User's Guide](https://bitsavers.trailing-edge.com/pdf/digitalResearch/cpm/XLT86_Users_Guide_Sep81.pdf),
sections 1–3; xlt88 claims come from current code and recorded tests. “Not
documented” is limited to that version. VTL-2 checks expected result tokens,
not exhaustive equivalence. No comparative size/speed benchmark is claimed.

## Tests and documentation

`make test` runs regression; `make corpus` inventories examples; `make vtlbench`
runs the VTL-2 comparison. Python 3 and the assembler/emulator tools above plus
emu2 and bin2cmd must be on PATH. Overrides are in tests/toolrun.py.
Tests print results; a report-producing test accepts an optional output path,
for example `python3 tests/vtlbench.py /tmp/vtlbench.md`. Normal tests do not
recreate documentation. Keep repository Markdown within 800 lines total.

[Status and evidence](status.md) · [Architecture](design.md) ·
[Source dialects](inputs.md) · [Annotated COM](com.md) ·
[Labels/EQU state](labels.md) · [OS contracts](api.md).
