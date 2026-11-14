# Native API translation coverage

The source API is CP/M 2.2 BDOS. The targets are CP/M-86 1.1
(`INT E0h`) and IBM PC DOS 1.00 (`INT 21h`, plus `INT 20h` for exit).
The table records current translation and the work required for each call.

References

- [Microsoft MS-DOS Encyclopedia, system calls](https://www.pcjs.org/documents/books/mspl13/msdos/encyclopedia/section5/)
- [CP/M BDOS system calls](https://www.seasip.info/Cpm/bdos.html)
- Local CP/M-86 kernel, `cpm86-kernel/bdos.a86`, dispatch table `IXMAIN`
- Local IBM PC DOS 1.00 `IBMDOS.COM`, interrupt entry and dispatch table
- Local `pcdos11-hacking/dos.asm`, DOS 1.25-derived dispatch and service bodies.
  Use this for comparison and verify service availability against 1.00.

| BDOS decimal | Service | CP/M-86 1.1 lowering | PC DOS 1.00 lowering |
| --- | --- | --- | --- |
| 0 | Terminate | Implemented | Implemented via INT 20h |
| 1, 2, 9 | Console input, output, string | Implemented | Implemented |
| 3 | Reader input | Native lowering; all emitters assemble; device execution unverified | Native AH=03h; byte result copied to A/L; device execution unverified |
| 4, 5 | Punch/auxiliary and list output | Native lowering; all emitters assemble; device execution unverified | Native AH=04h/05h lowering; all emitters assemble; device execution unverified |
| 6 | Direct console I/O | Implemented; FF polls input, FE output uses function 2 | Implemented via AH=06h, result copied to A and L |
| 7, 8 | Get/set IOBYTE | Native calls; four emitters assemble, routing-state execution unverified | Port device selection explicitly |
| 10 | Buffered input | Implemented; empty/full-capacity input and guard byte tested | Native AH=0Ah buffer adapter for capacities 0..254; count/data copied back without CR; capacity 255 reports an error and exits; native editing/completion policy |
| 11 | Input status | Implemented; native status result | Implemented via AH=0Bh, result copied to A and L |
| 12 | BDOS version | Implemented | Port the version-dependent branch explicitly |
| 13 | Reset disks | Native reset/select A/DMA/search sequence; execution tested | Native reset/select A/DMA/search sequence; execution tested |
| 14 | Select drive | Implemented via native function 14 | Implemented via AH=0Eh; native drive count is not a CP/M status |
| 15, 16, 22 | Open/close/create | Implemented | Audited private or first-default FCB; safe create probe |
| 19, 23 | Delete/rename | Implemented; success/missing-file tests | Implemented through native name fields; success/missing/delete-wildcard tests; rename collision execution unverified |
| 17, 18 | Directory search | Implemented; wildcard/DMA entries/end-of-search tested | Pending: native DOS search-result representation needs translation |
| 20, 21 | Sequential read/write | Implemented; translated copies through 260 records, EOF and A=L checks | Audited private or first-default FCB; padded partial-read status handled |
| 24 | Login vector | Implemented; native BX maps to source HL; low/high bitmap execution checks | Port drive enumeration explicitly |
| 25 | Current drive | Implemented | Implemented via AH=19h |
| 26 | Set DMA address | Implemented for the current DMA segment | Implemented via AH=1Ah (DTA) |
| 27, 31 | Allocation map, disk parameter block | Native ES:BX result emitted and ABI tested; explicit pointer-consumer port review required | Port allocation and geometry consumers to the FAT interface |
| 28, 29 | Write-protect drive, read-only vector | Native lowering; zero bitmap executed; stateful protection execution unverified | Port write-protection policy explicitly |
| 30 | Set file attributes | Native lowering; read-only set/clear and missing file tested; system/archive execution unverified | Port attribute operations explicitly |
| 32 | Get/set user area | Native lowering and assembly tested; user-state execution unverified | Port user-area naming or directory policy explicitly |
| 33, 36 | Random read, set random record | Implemented; position and EOF tested | Audited private or first-default FCB; bookmarks, extent crossing, repeat reads, partial-record padding and EOF tested; sequential continuation execution unverified |
| 34, 35 | Random write, file size | Implemented; extent crossing and size tested | Audited private or first-default FCB; random overwrite and rounded 24-bit size tested; allocation-error execution unverified |
| 37 | Reset selected drives | Native bitmap reset; all emitters assemble; state execution unverified | Port selected-drive reset behavior explicitly |
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

Function 13 composes native reset, select A, set DMA and directory search.
The CP/M 2.2 source (`func13` and `initial2` in
`~/Projects/CPM/sourcebundles/cpm22src_unix/os3bdos.asm`) establishes drive A,
DMA 0080h and A=FFh if a current-user filename starts with `$`, otherwise zero;
the user number is unchanged. CP/M-86 MRSDSY alone does not establish that
contract. The generated search uses extent/module wildcards on CP/M-86; DOS
uses an extended native FCB with hidden/system attribute mask 6. A private
128-byte search buffer protects the program's old DMA area, then DMA is set
to 0080h. The result is returned in A=L. No substitute filesystem state is kept.
`tests/allreset.py` exercises repeated calls, drive/DMA/results and untouched
files on all eight emitter/OS combinations. Native read-only reset state and
physical media behavior remain unverified.

CP/M-86 functions 7/8 use native IOBYTE get/set with ES preserved. The kernel
routes these to BIOS VGTIOB/VSTIOB. `tests/iobyte.py` assembles all four flavors;
`CPM_IOBYTE=1` additionally requires routing-state execution on a capable runner.
The current emulator cannot validate it. DOS device selection remains an
explicit port rather than a synthetic IOBYTE variable.

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

## Reader input and login vector

Reader input (3) uses native INT E0h or DOS AH=03h/INT 21h. DOS AL is mirrored
to BL for the source A/L convention. The supplied CP/M-86 MREDER/BTPRST path
and DOS READER/AUXIN path provide the native contracts. Native device selection
and interruption policy apply; no CP/M IOBYTE emulation is introduced on DOS.
`tests/devices.py` assembles this call but does not claim hardware execution.

CP/M-86 function 24 uses MRTLIV/WDPRST's BX login bitmap, already the translated
HL pair. `tests/login.py` executes all four emitters with mapped A/B/C/P drives
and checks bits in both bytes. DOS has no matching native login-vector contract
in this implementation, so its source function 24 remains explicitly unsupported.

## Audited DOS random reads

Functions 33 and 36 use DOS AH=21h and AH=24h respectively, available since
DOS 1.0 (Microsoft MS-DOS Encyclopedia, section V). The existing whole-program
opaque-FCB audit applies: dedicated storage, bounded name initialization, no
source access to repurposed metadata. Function 36 derives the random position
from native sequential fields; function 33 returns A=L and normalizes DOS
partial-record status 3 to CP/M success, retaining the zero-padded record.
Function 36 defines no byte return value. Identifiable incompatible field accesses remain rejected. Private FCBs use
the strict storage audit; the first default FCB uses the native-memory contract
described below.

`tests/dosrand.py` executes bookmarks, repeated reads, extent crossing, padding,
and EOF through every emitter. DOS requires random read to set the sequential
position without incrementing it. The installed emulator leaves that position
unchanged; the local DOS kernel FINRND/SETNREX and Microsoft documentation agree
on the native contract. `DOS_RSEQ=1` enables the strict continuation test on a
conforming runner. No workaround is added to generated code.

## Native attributes, users and drive protection

CP/M-86 functions 28/29 use MWPDSK/MGTROV: protect the current drive and return
the read-only bitmap in BX (source HL). `tests/protect.py` executes the zero
bitmap case and assembles the stateful case through all emitters. The installed
emulator lacks protection state; `CPM_PROT=1` enables the stateful check.
Function 30 tests read-only attribute set/clear and missing-file failure; host
files do not verify system/archive bits. Function 32 assembles query/select/
restore; `CPM_USER=1` requires a runner maintaining native user-area state.

## First default FCB and native memory

DOS lowering accepts the first default FCB at 005Ch for functions 15, 16,
20, 21, 22 and 33–36. DOS provides its storage through 007Fh; the command
name prefix is compatible, so no copy, shadow table or runtime dispatcher is
needed. The second default FCB and mixed default/private file calls remain
unsupported in this path because their storage relationships need analysis.

Explicit metadata references are diagnosed, including word accesses crossing
from the name prefix into metadata, arithmetic aliases and straight-line
register-derived addresses. Arbitrary runtime addresses see native target
memory. This is not a claim to detect every alias across calls or control-flow
joins, nor to preserve CP/M structures for scripts that inspect raw memory.
Private FCB relocation retains the stricter whole-program audit.

Unchanged VTL-2 now passes interactive and script-file acceptance on DOS with
all four emitters. Script output matches the CP/M-86 xlt86 reference exactly
apart from CP/M's additional final warm-boot CR/LF. Tests include stored programs,
empty/missing files, multi-record scripts and Ctrl-Z termination.

## Random write, file size and selected-drive reset

DOS functions 34/35 use native AH=22h/23h. Random write retains the native
bookmark and maps DOS allocation failure 1 to CP/M random-write failure 2.
DOS does not distinguish CP/M directory-full status 5 from other allocation
failure; this detail is not reconstructed. Disk-full/error-path execution is
unverified. Sequential continuation after random write shares the installed
emulator limitation described for random read; `DOS_RSEQ=1` enables its strict
check in `tests/dosmore.py`. Repeated writes, actual host-file contents and extent crossings
are tested through all four emitters with private and first-default FCBs.

File size explicitly establishes a 128-byte record length, including unopened
FCBs. R0..R2 are cleared before the native lookup so a missing file retains a
zero count as in the CP/M kernel. DOS's AL status is mirrored to L; CP/M 2.2
file size does not define that byte result. Direct reads of the three-byte
random field are supported; private objects require a relocatable object-plus-
constant expression and sufficient source storage. Boundary-crossing reads
and incompatible metadata writes remain diagnosed. Tests cover zero length,
partial records, missing files and counts exceeding 65535, without modifying
input files. Source field writes and arbitrary random seeks are not newly enabled.

CP/M-86 function 37 uses MRSTDV: the DX bitmap clears selected login and
read-only bits, preserving unselected drives. `tests/reset.py` assembles a
low/high-bit reset and current-drive preservation scenario with every emitter.
Set `CPM_RESET=1` only on a runner supporting native drive-protection state;
the installed emulator does not implement this call. No runtime substitute
is generated.

## Far-result entry points: 27 and 31

Both calls now emit native INT E0h and retain ES:BX. The kernel dispatches
27 to MGTALM and 31 to MGDPBA, returns the offset through OTPARA, and restores
ES from CURSEG on exit. Restoring the caller's old ES would destroy the native
pointer, so these two calls deliberately bypass the ordinary ES save/restore.
No copy of the allocation map or DPB, synthetic near pointer or emulated OS
memory is introduced.

Each call emits a diagnostic and an assembly comment requiring manual review
of pointer consumers. Existing source loads still use their normal segment;
no ES override is inferred. Review all pointer copies, stack saves, arithmetic,
memory dereferences and lifetimes across subsequent native calls. A new far
result replaces the previous ES value; multiple live far pointers need an
explicit segment-storage strategy. Other existing service wrappers preserve ES.

`tests/farptr.py` exercises both native results through all four assemblers.
Its explicit test-only target adjustment poisons incoming ES, reads through
ES:BX, compares segment/offset/data with a repeated raw native call, and checks
ES preservation through an ordinary console-status call. These checks validate
the native ABI and the manual port boundary, not automatic source-pointer
conversion or every real disk geometry. DOS continues to reject these calls.

The assembly-only tests/bdosall.py fixture covers the full entry set through
all four emitters. There are now 39/39 native CP/M-86 entry-point lowerings. Two of those (27/31)
require pointer-consumer port review; this is not 39/39 automatic flat-memory
source contracts or complete behavioral validation. Remaining execution limits
in the table, including IOBYTE/protection/user state and zero fill, still apply.


## CP/M-86 native pass-through policy

Unknown selectors and known numbers outside the supported CP/M 2.2 set now
emit raw INT E0h through the backend's optional native hook. Unmatched memory
candidates use the same fallback. Diagnostics and assembly comments require
port review. No ES restoration, result adaptation or function-availability
claim accompanies that raw call. Known supported entries keep their explicit
lowerings; DOS continues to reject unknown/unsupported services.

This distinction matters for 6 (CP/M-86 FE means status) and 13 (native reset
alone does not establish the translated source drive/DMA contract). It also
matters for far results, termination parameters and native extensions such as
46. Pass-through is not additional automatic source-service coverage.
`tests/native.py` exercises these boundaries and tail returns; `farptr.py`
checks both known and dynamic far results using explicit test-only consumers.

## DOS buffered input representation

BDOS 10 now uses native DOS AH=0Ah with a private 257-byte native buffer per
call site. For caller capacities 1..254 the native capacity is one larger,
accounting for DOS's CR. Only the returned count and actual characters are
copied back; the caller's capacity and unused/guard bytes remain unchanged.
Capacity zero returns an empty count without consuming input. Capacity 255
cannot be represented with DOS's 8-bit maximum including CR: it prints an
explicit diagnostic and terminates, rather than silently truncating capacity.

Line editing and CR completion remain native DOS policy. CP/M's full-buffer
immediate completion, editing keys and parity/control handling are not emulated.
Each lowering emits a source diagnostic describing these differences. This
raises DOS entry coverage to 25/39, not full behavioral equivalence.

`tests/conbuf.py` checks empty, partial, full and repeated reads, capacities
0/1/4/254, exact caller bytes and capacity-255 rejection through all emitters.
The installed emulator's DOS 0Ah accepts up to its maximum rather than maximum
minus one, unlike the DOS source contract. Tests verify adapter bounds and
representations; native interactive editing/overflow behavior still needs a
real-DOS acceptance run. The adapter clamps an excessive native returned count.
