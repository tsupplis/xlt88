# VTL-2 comparison benchmark

The source and direct xlt86 reference are used unchanged. No source-specific patches are applied.

Source SHA256: `215bff18ad99c90acde85cfa9a18daea7aeeae06391dacb91aced92b94a5719b`
Reference SHA256: `809db9dd5d2b1a461606e328482514c0c4e31e9caeb6c5663bf953f59e02148a`

## Reference execution

ASM86 1.1N2 assembly and GENCMD combined-image packaging succeeded.
Execution uses emu2 in 8088 mode: startup, addition (579), variable assignment/multiplication (42), subtraction (667), wide multiplication (65025), division (257), 16-bit wrap (0), and clean exit passed.

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
?=1234-567
667
OK
?=255*255
65025
OK
?=65535/255
257
OK
?=65535+1
0
OK
>=
```

Reference end of code: **078Bh**; fixed variable origin: **0800h**.

## xlt88 results

| Target | Emitter | Result |
| --- | --- | --- |
| cpm86 | nasm | reference transcript checks passed |
| cpm86 | asm86 | reference transcript checks passed |
| cpm86 | masm3 | reference transcript checks passed |
| cpm86 | drasm86 | reference transcript checks passed |
| msdos | nasm | reference transcript checks passed |
| msdos | asm86 | reference transcript checks passed |
| msdos | masm3 | reference transcript checks passed |
| msdos | drasm86 | reference transcript checks passed |

A blocked result is not an execution pass. The table records each current result independently. Both targets are required acceptance gates; arbitrary memory uses native target semantics.
The fixed origin is required by eight-bit variable indexing. The translated source and variable location remain unchanged; only proven general code-size optimisations are applied.
