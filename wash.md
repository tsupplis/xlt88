# WASH15 acceptance

The supplied reference is xlt86 output with deliberate porting patches.
Both it and the original 8080 source are used unchanged.

Source SHA256: `76d4229a49fdf2ae7598f5328fb0e856f6f9972dbcc98cbbb468f7cbd59f0bc7`
Reference SHA256: `51fc25cb6fd99cf33757789b049870d6530d7bba65522783b2221867a33cd4b8`

## Reference scenarios

- missing: passed; fixture files unchanged.
- exit: passed; fixture files unchanged.
- browse: passed; fixture files unchanged.
- help: passed; fixture files unchanged.
- view: passed; fixture files unchanged.

## Unchanged-source translation

Reference CMD: 3840 bytes. Sizes include container/padding; they are not instruction counts.

| Target | Result | CMD bytes | Size / reference |
| --- | --- | ---: | ---: |
| cpm86/nasm | all five reference transcripts match | 4608 | 1.20 |
| cpm86/asm86 | all five reference transcripts match | 4352 | 1.13 |
| cpm86/masm3 | all five reference transcripts match | 4608 | 1.20 |
| cpm86/drasm86 | all five reference transcripts match | 4608 | 1.20 |
| msdos | blocked by FCB representation; no execution claimed | — | — |

```text
xlt88: DOS FCB analysis: unrecognized subroutine may expose FCB storage
```

## Remaining porting work

- Interrupt policy: the original disables interrupts; the patched reference explicitly enables them after stack setup. Original DI is preserved. Emulator transcript passes do not validate hardware interrupt policy.
- Selector dispatch uses direct-store candidates with checks on every arm. Unmatched CP/M-86 selectors use warned native pass-through; no memory alias proof is claimed.
- Native reset/select/DMA/search implement reset without simulated drive state. Reset state tests live in allreset.py; protected-drive state still needs emulator support.
- Inline strings and code-label data access are exercised by these transcripts. Seven additional file-operation/cancellation comparisons are in washfile.md; successful copy has separate reference limitations. Devices and drive-switch workflows remain.
- DOS needs directory-result and explicit FCB-field translation.

No program-name exception or source patch has been added.
