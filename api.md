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
| 3 | Reader input | Native device call to verify | AH=03h, compare device semantics |
| 4, 5 | Punch/auxiliary and list output | Native lowering; all emitters assemble; device execution unverified | Native AH=04h/05h lowering; all emitters assemble; device execution unverified |
| 6 | Direct console I/O | Implemented; FF polls input, FE output uses function 2 | Implemented via AH=06h, result copied to A and L |
| 7, 8 | Get/set IOBYTE | Native calls to verify | Port device selection explicitly |
| 10 | Buffered input | Implemented; empty/full-capacity input and guard byte tested | Pending: DOS maximum includes its CR terminator; do not pass a CP/M-sized buffer unchanged |
| 11 | Input status | Implemented; native status result | Implemented via AH=0Bh, result copied to A and L |
| 12 | BDOS version | Implemented | Port the version-dependent branch explicitly |
| 13 | Reset disks | Pending: compare boot/current-drive, DMA and reset-result semantics | Pending: AH=0Dh alone does not reproduce the CP/M reset contract |
| 14 | Select drive | Implemented via native function 14 | Implemented via AH=0Eh; native drive count is not a CP/M status |
| 15, 16, 22 | Open/close/create | Implemented | Implemented for proven opaque FCB uses; safe create probe |
| 19, 23 | Delete/rename | Implemented; success/missing-file tests | Implemented through native name fields; success/missing/delete-wildcard tests; rename collision execution unverified |
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
adding them. The supplied DOS 1.25 kernel dispatcher accepts AH up to 2Dh; it is a
corroborating source, not proof of DOS 1.00 availability.
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

## Console additions and recognition

Function 10 on CP/M-86 follows MRDCBF/RDCNBF in the supplied kernel: input
DX points to maximum/count/data, and the backend preserves ES around INT E0h.
Tests exercise empty input and a full three-character buffer without changing
the byte beyond its capacity. Function 11 uses native status on both targets;
DOS mirrors AL into BL for the source A/L result convention. Installed emu2
returns not-ready for redirected stdin, which is the executed status case;
`tests/console.py` additionally checks ready status and direct input/output with
a terminal handle and injected keyboard input on all eight combinations.
Function 6 lowers directly to the native service, except CP/M-86 FE output:
its native function 6 reserves FE for status, so that byte uses function 2.

Recognition now tracks C through an explicitly whitelisted straight-line
sequence, with MVI C and LXI B as constant definitions. Labels, transfers,
unknown operations and writes to C/BC invalidate the proof. This is not
interprocedural or join-aware dataflow. DOS FCB analysis retains its separate
storage and provenance requirements; recognizing a function does not waive them.

## DOS name-only file services

Functions 19 and 23 lower to DOS AH=13h and AH=17h, mirroring AL into BL for
A=L. These operations consume compatible name fields: drive/name at FCB+0/+1,
and the new rename name at +17. They do not reinterpret CP/M allocation or
record metadata and do not require the opaque open-file conversion. Native DOS
namespace/device/attribute rules apply; this does not emulate CP/M user areas.

`tests/dosnames.py` executes exact deletion, wildcard deletion, missing-file
failure and rename success/missing-file failure on all four emitters. It checks
A=L, data preservation and guards around the name record. Native DOS rejects an
existing rename destination (RENAME/RENERR in the supplied DOS source). Installed
emu2 instead calls host `rename()` and overwrites it. Collision acceptance is
therefore explicitly unverified; set `DOS_RENAME=1` with a corrected emulator
to require the no-overwrite execution check. No emulator workaround is emitted.

## Drive selection

Function 14 uses zero-based drive in E/DL on both targets. Valid configured
native drives are tested; invalid/unavailable-drive behaviour remains native.
CP/M 2.2 defines no result for this operation, so the DOS drive-count result is
not advertised or converted as a source success/error byte. `tests/drives.py`
checks A/B/C selection and function 25's A=L result on all eight combinations,
then verifies that zero-drive name records affect files on the selected drive.
Distinct temporary A/B/C mappings and untouched files detect selection mistakes.

Function 13 remains blocked. The supplied CP/M-86 MRSDSY resets login/read-only
vectors and selects CURDRV; it does not itself reset the DMA offset. This needs
reconciliation with the CP/M-80 reset contract, as does DOS disk flush. No
substitute reset state or guessed return value is emitted.

## Auxiliary and list output

BDOS 4 and 5 now lower to native device output on both targets, passing E as
DL and defining no fabricated result. CP/M-86 uses INT E0h and preserves ES;
DOS uses AH=04h/05h and INT 21h. These select the target's native devices;
CP/M IOBYTE configuration is not emulated on DOS.

Contract evidence: the supplied CP/M-86 `bdos.a86` dispatch table and BLIST
handler; the supplied DOS `dos.asm` PUNCH/LIST handlers; and Microsoft's
[MS-DOS Encyclopedia, functions 04h and 05h](https://www.pcjs.org/documents/books/mspl13/msdos/encyclopedia/section5/),
which explicitly lists availability from DOS 1.0. The local DOS source is
1.25, so version availability is checked separately.

`tests/devices.py` translates and assembles both calls in all eight combinations.
Device output execution is unverified; the installed CP/M emulator does not
implement these two BDOS entries. No emulator workaround is emitted.

## Proven service sets

Recognition follows direct branches and direct-call inputs, tracking up to
four possible byte values per register. A shared routine can receive several
known service values. Return registers after a call remain unknown. Unknown
paths or sets exceeding the bound are rejected; no catch-all runtime BDOS
handler is generated. Every possible service must have a native lowering.

DOS opaque-FCB services still require the separate whole-program FCB audit.
Mixed service sets do not bypass that audit. An FCB address may now be loaded
before intervening proven DE-preserving register moves, immediate loads or NOPs;
control transfers and possible pointer exposure remain barriers.
