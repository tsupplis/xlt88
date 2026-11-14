# DISK7 reference acceptance

The supplied ASM86 reference includes manual porting patches. Both inputs remain unchanged.

Source SHA256: `fb55c1cdb24c5df03afb7b3a85ce318d2efe97450a2ae2603a5a1537823679b0`
Reference SHA256: `a1c87a89c80f0eaf0437577302c6d29b962b9e9ead73d1889ac5effd03f49d99`

Reference CMD size: 5248 bytes (including container/padding).

## Reference scenarios

- exit: passed twice with identical transcripts; fixture files unchanged.
- browse: passed twice with identical transcripts; fixture files unchanged.
- length: passed twice with identical transcripts; fixture files unchanged.
- view: passed twice with identical transcripts; fixture files unchanged.

## Unchanged-source results

| Emitter | Result |
| --- | --- |
| nasm | translates and assembles; execution requires explicit port review |
| asm86 | translates and assembles; execution requires explicit port review |
| masm3 | translates and assembles; execution requires explicit port review |
| drasm86 | translates and assembles; execution requires explicit port review |

Representative diagnostics:

```text
xlt88:/Users/thierry/Projects/CPM86/xlt88/examples/disk7.mac:1230: native BDOS pass-through: CL selects the target service; native results/segments apply, including differences for functions 6/13; port review required
xlt88:/Users/thierry/Projects/CPM86/xlt88/examples/disk7.mac:1238: native BDOS pass-through: CL selects the target service; native results/segments apply, including differences for functions 6/13; port review required
xlt88:/Users/thierry/Projects/CPM86/xlt88/examples/disk7.mac:1969: BDOS 31 returns native ES:BX; manual port review required for pointer consumers (segment overrides are not inferred)
xlt88:/Users/thierry/Projects/CPM86/xlt88/examples/disk7.mac:1993: native BDOS pass-through: CL selects the target service; native results/segments apply, including differences for functions 6/13; port review required
xlt88:/Users/thierry/Projects/CPM86/xlt88/examples/disk7.mac:2009: BDOS 27 returns native ES:BX; manual port review required for pointer consumers (segment overrides are not inferred)
```

Generated DISK7 is not executed: pointer segments, BIOS access and exit/stack assumptions still require review. No transcript equivalence is claimed.
Function 46 now passes through natively. The version-dependent branch is retained; its native availability and semantics remain the responsibility of the port author.
The reference ES overrides remain deliberate target adjustments, not application-specific translator rules.
