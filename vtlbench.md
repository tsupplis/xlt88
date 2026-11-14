# VTL-2 comparison benchmark

The source and direct xlt86 reference are used unchanged. No source-specific patches are applied.

Source SHA256: `215bff18ad99c90acde85cfa9a18daea7aeeae06391dacb91aced92b94a5719b`
Reference SHA256: `809db9dd5d2b1a461606e328482514c0c4e31e9caeb6c5663bf953f59e02148a`

## Reference execution

ASM86 1.1N2 assembly and GENCMD combined-image packaging succeeded.
Execution uses emu2 in 8088 mode: startup, addition (579), variable assignment/multiplication (42), and clean exit passed.

```text
VTL2 Interpreter 1.0

OK
?=123+456
579
OK
A=7

OK
?=A*6
42
OK
>=
```

Reference end of code: **078Bh**; fixed variable origin: **0800h**.

## xlt88 results

| Target | Emitter | Result |
| --- | --- | --- |
| cpm86 | nasm | blocked before execution |
| cpm86 | asm86 | blocked before execution |
| cpm86 | masm3 | blocked before execution |
| cpm86 | drasm86 | blocked before execution |
| msdos | nasm | blocked before execution |
| msdos | asm86 | blocked before execution |
| msdos | masm3 | blocked before execution |
| msdos | drasm86 | blocked before execution |

A blocked result is not an execution pass. CP/M-86 currently exceeds the fixed 0800h region; DOS additionally requires proven default-FCB handling.
The fixed origin is required by eight-bit variable indexing. Shrinking code requires proven general optimisations; moving the block is not assumed safe.

### cpm86/nasm

```text
(['nasm', '-Werror', '-f', 'bin', 'vtl.asm', '-o', 'vtl.com'], b'', b'vtl.asm:1834: error: TIMES value -330 is negative\n')
```

### cpm86/asm86

```text
(['cpm86_asm86', 'vtl.a86'], b'\r\nCP/M-86 / CCP/M-86 8086 ASSEMBLER VER 1.1N2\r\nEND OF PASS 1\r\nEND OF PASS 2\r\n** ERROR NO: 10 **  NEAR: "XOVFLOW"   UNDEFINED ELEMENT OF EXPRESSION\r\n 095D 0000                      dw x_ovflow ; translated code exceeds ORG\r\nEND OF ASSEMBLY.  NUMBER OF ERRORS:   1.  USE FACTOR: 11%\r\n\r\n', b'ERR: cpm86_asm86: fatal assembly error(s) detected; removing vtl.h86\n')
```

### cpm86/masm3

```text
(['pcdev_masm3', 'vtl.asm,vtl.obj;'], b'Microsoft MACRO Assembler  Version 3.00               \r\n(C)Copyright Microsoft Corp 1981, 1983, 1984\r\n\r\n 0953  0000 U                   \tdw x_ovflow ; translated code exceeds ORG\r\n E r r o r   ---\t9:Symbol not defined            \r\n 0955     01 [                  \tdb (2048-($-x_entry+100h)) dup(0)\r\n E r r o r   ---\t72:Illegal value for DUP count   \r\n\r\n34534 Bytes free   \r\n\r\nWarning Severe\r\nErrors\tErrors \r\n0\t2\r\n', b'')
```

### cpm86/drasm86

```text
b'--------------------------------------------------\r\nRASM-86 Assembler   12-Mar-87  PC-DOS Version 1.4a\r\nSerial No. XXXX-0000-654321    All Rights Reserved\r\nCopyright (C) 1982-86       Digital Research, EDC.\r\n--------------------------------------------------\r\n\r\nEnd of Pass 1\r\nEnd of Pass 2\r\n** ERROR NO: 10 **  NEAR: "X_OVFLOW"   UNDEFINED ELEMENT OF EXPRESSION\r\n\x0cPC-DOS RASM-86 1.4a  Source: VTL.A86                                                                         Page  42\r\n\r\n\r\n 095D 0000                        dw x_ovflow ; translated code exceeds ORG\r\n** ERROR NO: 31   Current segment is too big (over 64K)\r\n 095F                             rb (2048-(p_1-x_entry+100h))\r\n\r\nCODE  0095F\r\n\r\nEnd of assembly.  Number of errors:   2.  Use factor: 14%\r\n'
```

### msdos/nasm

```text
(['/Users/thierry/Projects/CPM86/xlt88/xlt88', '--dialect=intel', '--target=msdos', '--emit=nasm', '/var/folders/hf/9txlstvj7tv6v7bgyh95pszm0000gn/T/tmpn14kpv1e/vtl.80', 'vtl.asm'], b'', b'xlt88: DOS FCB analysis: file call needs a proven FCB address in DE\n')
```

### msdos/asm86

```text
(['/Users/thierry/Projects/CPM86/xlt88/xlt88', '--dialect=intel', '--target=msdos', '--emit=asm86', '/var/folders/hf/9txlstvj7tv6v7bgyh95pszm0000gn/T/tmpn14kpv1e/vtl.80', 'vtl.a86'], b'', b'xlt88: DOS FCB analysis: file call needs a proven FCB address in DE\n')
```

### msdos/masm3

```text
(['/Users/thierry/Projects/CPM86/xlt88/xlt88', '--dialect=intel', '--target=msdos', '--emit=masm3', '/var/folders/hf/9txlstvj7tv6v7bgyh95pszm0000gn/T/tmpn14kpv1e/vtl.80', 'vtl.asm'], b'', b'xlt88: DOS FCB analysis: file call needs a proven FCB address in DE\n')
```

### msdos/drasm86

```text
(['/Users/thierry/Projects/CPM86/xlt88/xlt88', '--dialect=intel', '--target=msdos', '--emit=drasm86', '/var/folders/hf/9txlstvj7tv6v7bgyh95pszm0000gn/T/tmpn14kpv1e/vtl.80', 'vtl.a86'], b'', b'xlt88: DOS FCB analysis: file call needs a proven FCB address in DE\n')
```
