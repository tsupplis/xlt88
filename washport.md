# WASH explicit native allocation port

This is acceptance of a test-only target port, not unchanged-source translation.
The original source/reference and production translator are unchanged by these adjustments.
Reduced allocations are simulated by overriding the base-page descriptor in disposable test binaries: emu2 grants at least 64 KiB. Sixteen-byte canaries check writes beyond the simulated limit.

| Emitter | Copy and replace | Directory bounds |
| --- | --- | --- |
| nasm | 18 exact-byte cases passed | overflow rejected; files unchanged |
| asm86 | 18 exact-byte cases passed | overflow rejected; files unchanged |
| masm3 | 18 exact-byte cases passed | overflow rejected; files unchanged |
| drasm86 | 18 exact-byte cases passed | overflow rejected; files unchanged |

Copy/replace cases cover empty, one-record and 513-record files at image size + 256 bytes, 16 KiB and 64 KiB allocations.
The port enables interrupts after selecting the program stack, reads the native code-group limit, bounds directory appends with one record reserved, and computes capacity by subtraction/division.
The one-byte conservative limit follows the maintained port. BDOS read/write failure cleanup and DOS FCB conversion remain separate work.
