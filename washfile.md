# WASH file-operation comparison

Source and reference are unchanged. Each run starts from isolated A/B fixtures.
Complete transcripts and file inventories/bytes are compared independently.

## Passing comparisons

| Emitter | Result |
| --- | --- |
| nasm | all seven transcripts and file snapshots match |
| asm86 | all seven transcripts and file snapshots match |
| masm3 | all seven transcripts and file snapshots match |
| drasm86 | all seven transcripts and file snapshots match |

Cases: rename, rename cancel, rename duplicate, delete, copy decline, copy same drive, copy invalid drive.

## Copy completion diagnostic

These probes use independent expected file contents and a ten-second bound. Reference timeouts are not matches.

| Program | Copy | Replace |
| --- | --- | --- |
| wasref | timed out; no completion claimed | timed out; no completion claimed |
| wnasm | timed out; no completion claimed | timed out; no completion claimed |
| wasm86 | completed with matching bytes | completed with matching bytes |
| wmasm3 | completed with matching bytes | completed with matching bytes |
| wdrasm86 | completed with matching bytes | completed with matching bytes |

ASM86, MASM3 and DRASM86 also copy 513 records with exact bytes and source preservation. This is independent expected-result acceptance, not reference equivalence.

Copy probes check expected destination bytes independently; a reference timeout is never counted as a transcript match.
Both sources retain the BDOS+1 memory-limit lookup and an unchecked 16-bit buffer-growth loop.
A native allocation-aware port needs separate acceptance; the maintained sibling port is not substituted here.
Console-buffer input uses stdin for these cases because this emulator handles BDOS 10 separately from its key-script input.
