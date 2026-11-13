# xlt88

xlt88 translates Intel 8080 assembly source to NASM assembly for the 8086
or 8088. Its OS targets are IBM PC DOS 1.0 and CP/M-86 1.1.

## Build and run

```sh
make
./xlt88 --target=msdos --dialect=intel source.asm dos.asm
nasm -f bin dos.asm -o program.com

./xlt88 --target=cpm86 --dialect=intel source.asm cpm.asm
nasm -f bin cpm.asm -o program.bin
bin2cmd program.bin program.cmd
```

The two-argument invocation defaults to MS-DOS and the Intel dialect.
For CP/M-86, use the default base-page insertion in the cpm86-cmdtools
`bin2cmd` utility to package a single-code-group, 8080-model CMD.
Both listings use origin 100h and start with the translated source.

## OS calls

The backend emits the native instruction sequence at each recognized
transfer. A supported BDOS call uses `MVI C, constant` immediately
before an unlabelled transfer to address 5. Comments and blank lines
can appear between them. Addresses and service numbers can use simple
forward EQU aliases. A labelled transfer can have another caller,
so it requires a source edit before automatic translation.

| Source operation | DOS 1.0 | CP/M-86 1.1 |
| --- | --- | --- |
| BDOS 1, 2, 9 | Set AH to the service number and use INT 21h | INT E0h with CL already set, saving and restoring ES |
| Transfer to 0, RST 0, or BDOS 0 | INT 20h | INT E0h with CL and DL set to zero |

Other BDOS services, including FCB I/O, and ambiguous service numbers
produce source-line errors for manual porting. Review register and flag
dependencies against the native OS interface. Use an explicit warm boot
for portable termination. RET remains a near return. On CP/M-86,
`LXI SP` and `SPHL` select the program's data segment for SS before
setting SP. Programs that choose a private stack must reserve its
storage in the source.

Direct CALL and JMP targets, including conditional forms, accept
simple labels, numeric literals, and EQU chains ending in either.
Compound target expressions such as `0+5` require a source edit.

## Source dialect and data layout

The Intel dialect accepts column-one labels and indented instructions.
An initial `ORG 100h` is optional. Use a standalone `ALIGN 2` before a
word table or buffer to align its label. Alignment inserts zero padding
and accepts powers of two up to 256. Ordinary data declarations keep
their original layout.

Unknown mnemonics and nonzero RST vectors produce source-line diagnostics.
Preprocess macros and compound lines using `!` before translating.
Operands can use Intel's `AND` expression operator, for example
`(TABLE AND 0FFH)`. Expression references to source labels use their
offsets in the flat image. The emitter escapes source labels so that
names such as `CPU` can coexist with NASM keywords.

## Code and tests

`xlt88.c` handles instruction translation and symbol resolution. Each
opcode handler takes the current translation context, which owns the
statement, output, symbol table, diagnostics, and selected backend.
`dialect.h` defines the parsed statement interface and `dialect.c`
implements Intel syntax. The translator takes a backend and a dialect
independently. `msdos.c` and `cpm86.c` emit native OS instructions
through the interface in `backend.h`.

Run `make test` with NASM, emu2, and bin2cmd on PATH. The script accepts
`EMU2` and `BIN2CMD` overrides. It checks both Hello World binaries,
the generated native calls, diagnostics, exact packed and aligned data,
the 8080 auxiliary carry produced by ANA and ANI through PSW and DAA,
and the Microcosm diagnostic translated from `examples/tst8080.asm`
for each target.
It also runs both native FCB copy examples across a 128-record boundary.

`make test` checks that the assembled fixtures collectively cover every
8080 mnemonic in the translator table (78 of 78 today). The hardware
instruction fixture checks the emitted bytes for `DI`, `EI`, `HLT`,
`IN`, and `OUT`. It runs `NOP` followed by `RST 0` on both targets
and separately checks rejection of `RST 1` through `RST 7`. This is
mnemonic coverage. Operand forms, flag combinations, and hardware side
effects require separate tests.

## Native FCB copy examples

```sh
nasm -f bin examples/copy-dos.asm -o xcopy.com
nasm -f bin examples/copy-cpm86.asm -o xcopy.bin
bin2cmd xcopy.bin xcopy.cmd
```

Run `XCOPY INPUT.DAT OUTPUT.DAT` with two exact 8.3 filenames and a
fresh destination. A failed copy may leave a partial output file.
The programs copy 128-byte records. DOS writes a zero-padded record
when the final read reports a partial record.

Both examples copy the overlapping command-line FCB names into private
buffers. The DOS port sets the DTA and handles DOS status 3. The CP/M-86
port sets the DMA address and preserves ES around each returning BDOS
call. Automatic FCB translation still requires a way to establish that
the source uses FCB fields and read statuses compatible with its target.
