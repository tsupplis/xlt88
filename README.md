# xlt88

Native 8080-to-8086/8088 source translation for CP/M-86 1.1 and PC DOS 1.0.
No CP/M emulator or universal compatibility runtime.

## Use

```sh
make
./xlt88 --target=msdos --dialect=intel source.asm dos.asm
nasm -f bin dos.asm -o dos.com
./xlt88 --target=cpm86 --emit=asm86 source.asm cpm.a86
cpm86_asm86 cpm
cpm86_gencmd cpm 8080
```

Defaults: Intel input, DOS target, NASM output. Extensions do not select dialects.
Readers: intel, m80, mac, rmac, com. Emitters: nasm, asm86, masm3, drasm86, 8080.
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

## Intermediate 8080 ASM

`--emit=8080` emits normalized source before CPU/OS lowering, with macro
expansion and preferred names. Read it back using `--dialect=intel`.

```sh
./xlt88 --dialect=m80 --emit=8080 source.m80 review.asm
./xlt88 --dialect=mac --emit=8080 source.mac review.asm
./xlt88 --dialect=rmac --emit=8080 source.asm review.asm
./xlt88 --dialect=com --com-map=input.map --emit=8080 input.com review.asm
```

## Tests and documentation

`make test` runs regression; `make corpus` inventories examples; `make vtlbench`
runs the VTL-2 comparison. Python 3 and the assembler/emulator tools above plus
emu2 and bin2cmd must be on PATH. Overrides are in tests/toolrun.py.
Tests print results; a report-producing test accepts an optional output path,
for example `python3 tests/vtlbench.py /tmp/vtlbench.md`. Normal tests do not
recreate documentation. Keep repository Markdown within 500 lines total.

[Status and evidence](status.md) · [Architecture](design.md) ·
[Source dialects](inputs.md) · [Annotated COM](com.md) ·
[Labels/EQU state](labels.md) · [OS contracts](api.md).
