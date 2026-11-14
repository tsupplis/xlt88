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
| nasm | blocked: dynamic selectors and CP/M 3 function 46; pointer consumers require manual review |
| asm86 | blocked: dynamic selectors and CP/M 3 function 46; pointer consumers require manual review |
| masm3 | blocked: dynamic selectors and CP/M 3 function 46; pointer consumers require manual review |
| drasm86 | blocked: dynamic selectors and CP/M 3 function 46; pointer consumers require manual review |

Representative diagnostics:

```text
xlt88:/Users/thierry/Projects/CPM86/xlt88/examples/disk7.mac:1230: BDOS call needs a statically proven function in C: BDOS
xlt88:/Users/thierry/Projects/CPM86/xlt88/examples/disk7.mac:1238: BDOS call needs a statically proven function in C: BDOS
xlt88:/Users/thierry/Projects/CPM86/xlt88/examples/disk7.mac:1969: BDOS 31 returns native ES:BX; manual port review required for pointer consumers (segment overrides are not inferred)
xlt88:2469: BDOS function 46 needs a native port for cpm86
xlt88:/Users/thierry/Projects/CPM86/xlt88/examples/disk7.mac:1993: cannot translate BDOS transfer: BDOS
xlt88:/Users/thierry/Projects/CPM86/xlt88/examples/disk7.mac:2009: BDOS 27 returns native ES:BX; manual port review required for pointer consumers (segment overrides are not inferred)
```

No xlt88 DISK7 executable or reference-versus-generated transcript match is claimed.
CP/M 2.2 entry coverage does not include CP/M 3 function 46. The version-dependent branch is not eliminated.
The reference ES overrides remain deliberate target adjustments, not application-specific translator rules.
