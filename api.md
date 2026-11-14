# Native API translation coverage

The source API is CP/M 2.2 BDOS. The targets are CP/M-86 1.1
(`INT E0h`) and IBM PC DOS 1.00 (`INT 21h`, plus `INT 20h` for exit).
The table records current translation and the work required for each call.

References

- [CP/M BDOS system calls](https://www.seasip.info/Cpm/bdos.html)
- Local CP/M-86 kernel, `cpm86-kernel/bdos.a86`, dispatch table `IXMAIN`
- Local IBM PC DOS 1.00 `IBMDOS.COM`, interrupt entry and dispatch table
- Local `pcdos11-hacking/dos.asm`, DOS 1.25-derived dispatch and service bodies.
  Use this for comparison and verify service availability against 1.00.

| BDOS decimal | Service | CP/M-86 1.1 lowering | PC DOS 1.00 lowering |
| --- | --- | --- | --- |
| 0 | Terminate | Implemented | Implemented via INT 20h |
| 1, 2, 9 | Console input, output, string | Implemented | Implemented |
| 3–5 | Reader, punch, list | Native device calls to verify | AH=03h–05h, compare device semantics |
| 6 | Direct console I/O | Check CP/M 2.2 mode and status conventions | AH=06h, verify modes and return values |
| 7, 8 | Get/set IOBYTE | Native calls to verify | Port device selection explicitly |
| 10, 11 | Buffered input, input status | Native calls to verify | AH=0Ah–0Bh, check buffer and status conventions |
| 12 | BDOS version | Implemented | Port the version-dependent branch explicitly |
| 13, 14 | Reset disks, select drive | Native calls to verify | AH=0Dh–0Eh, check drive and reset semantics |
| 15, 16, 22 | Open/close/create | Implemented | Implemented for proven opaque FCB uses; safe create probe |
| 19, 23 | Delete/rename | Implemented; success/missing-file tests | Pending |
| 17, 18 | Directory search | Implemented; wildcard/DMA entries/end-of-search tested | AH=11h–12h, consume native DOS search results |
| 20, 21 | Sequential read/write | Implemented; translated copies through 260 records, EOF and A=L checks | Implemented for proven opaque FCB uses; padded partial-read status handled |
| 24 | Login vector | Native word result to verify | Port drive enumeration explicitly |
| 25 | Current drive | Implemented | Implemented via AH=19h |
| 26 | Set DMA address | Implemented for the current DMA segment | Implemented via AH=1Ah (DTA) |
| 27, 31 | Allocation map, disk parameter block | Far pointer. Port consumers to use segment and offset explicitly | Port allocation and geometry consumers to the FAT interface |
| 28, 29 | Write-protect drive, read-only vector | Native calls to verify | Port write-protection policy explicitly |
| 30 | Set file attributes | Verify native FCB attribute bits | Port attribute operations explicitly |
| 32 | Get/set user area | Verify native user number semantics | Port user-area naming or directory policy explicitly |
| 33–36 | Random read/write, file size, set random record | Implemented; extent crossing, size, position and EOF tested | AH=21h–24h, adapt native DOS FCB record fields |
| 37 | Reset selected drives | Verify drive bitmap and results | Port selected-drive reset behavior explicitly |
| 38, 39 | Reserved in CP/M 2.2 | Reserved | Reserved source calls |
| 40 | Random write with zero fill | Native lowering and emission tested; execution blocked by installed emulator | Port zero filling explicitly. DOS AH=28h is block write |

DOS 1.00 also has services with no BDOS 2.2 source operation, such as
taking an interrupt vector, parsing a filename into a DOS FCB, and
getting or setting the date and time. Expose those as native operations
when adding a source syntax for DOS-specific calls. Check their service
numbers and argument/result ABI against the 1.00 entry points before
adding them. The local DOS 1.00 INT 21h dispatcher accepts AH up to 2Dh.
That limit alone does not establish the behavior of every slot.

For each automatic lowering, test the input registers, documented return
registers, and relevant memory effects. File tests should exercise native
FCB layouts, sequential and random records, EOF, and error statuses. A
program that interprets CP/M-specific data needs an explicit port where
the DOS representation differs.

## CP/M-86 sequential file implementation

Functions 15, 16, 19–23 are now automatic native lowerings. The local kernel's
STWFCS and RSTFCB copy the 33-byte sequential FCB, including current record at
offset 32. Rename reads the new drive/name at offset 16. The source must
provide a valid CP/M FCB and DMA buffer, just as on CP/M-80. Returning calls
preserve the translated program's ES; native AL=BL supplies source A=L.

The tests/fileops.py suite executes translated source, not handwritten target
code. It covers empty and multi-extent copies, EOF, missing source files,
rename/delete success and missing-file failures, and byte return registers.
Disk-full, write-protection and device-error behavior is not yet exercised.
DOS functions 15, 16, 20–22 now support statically proven opaque FCB uses.
Other FCB uses remain diagnosed. Input is always CP/M; target selection must be
sufficient. The removed native-FCB mode did not satisfy that contract and is
not counted as supported translation. No compatibility runtime or shadow FCB
is part of the design.

## Random and search verification

`tests/randio.py` executes CP/M-86 functions 17, 18, 33, 34, 35 and 36,
including DMA directory entry indexing, wildcard enumeration, random access
at record 129, subsequent sequential position, file size and EOF. Function 40
is checked against the native kernel MWRITZ path and assembled, but the installed
emu2 dispatcher has no function 40 handler. Its execution test is available via
`CPM_ZFILL=1 make test` with a supporting emulator. Until that runs successfully,
zero-fill execution is explicitly unverified. Zero filling concerns newly
allocated blocks; it is not a promise that arbitrary sparse extents are filled.

## Proven opaque FCB lowering to DOS

fcb.c binds exact FCB addresses at recognized calls, checks dedicated DS storage,
recognizes the bounded name-copy idiom, and audits the program for unproved
accesses and escapes. CP/M metadata has no observed source uses in this subset,
so the target can assign native DOS metadata to the same object. Source
reservations shorter than 36 bytes grow; symbolic references follow the target
layout. No claim is made for CP/M extent/allocation-field consumers.

The DOS backend receives a file-call hook only after analysis succeeds. It
emits native open/close/read/write/create calls, AL-to-BL result handling,
partial-read normalization, and an existing-file probe before create to avoid
silent truncation. Tests use unchanged CP/M source with --target=msdos alone.
Metadata accesses, oversized copy loops, synthesized indirect aliases,
DMA-to-FCB, and unproved stack/pointer uses are rejection tests.
