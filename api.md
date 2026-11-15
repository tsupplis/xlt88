# Native API translation coverage

The source API is CP/M 2.2 BDOS. The targets are CP/M-86 1.1
(`INT E0h`) and IBM PC DOS 1.00 (`INT 21h`, plus `INT 20h` for exit).
The table records current translation and the work required for each call.
Native CP/M-86 lowering is CLOSED; behavioral exceptions remain below.

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

## Native contracts and limits

CP/M-86 uses native FCBs: 33 bytes sequential, 36 bytes random (R0–R2 at
33–35). Byte results map A=L; ordinary calls preserve ES. Functions 27/31
retain ES:BX and require explicit segment-aware consumer review. Function 6
adapts FE output; function 13 composes reset/select A/DMA/search. Version and
OS structures remain native. Unknown/extension selectors pass through with
warnings; this does not add automatic source-service coverage.

DOS requires audited CP/M FCB uses, never an input-layout assertion. Private
storage must be dedicated and bounded; safe 33-byte declarations may grow to
36. Name initialization is supported, arbitrary metadata copies/escapes are
not. The first default FCB uses native PSP storage; second/default-private
mixing and incompatible explicit fields remain rejected. Computed memory sees
native structures. DOS search-result conversion remains pending.

DOS BDOS 10 uses AH=0Ah and a private buffer: capacities 0..254, count/data
copied back without CR, caller bounds preserved. Capacity 255 reports an error
and exits. Editing and CR completion follow DOS. emu2's buffer-capacity behavior
differs from DOS, so native interactive overflow acceptance remains unverified.

Dedicated memory selector candidates are runtime-guarded, not alias proofs.
An unmatched CP/M-86 selector passes through; DOS terminates. Known services
retain their adaptations. Function 46 is outside the CP/M 2.2 entry set.

Strict state tests require a capable runner: CPM_IOBYTE=1, CPM_PROT=1,
CPM_USER=1, CPM_RESET=1 and CPM_ZFILL=1. Default assembly-only checks are not
runtime passes. Auxiliary/list devices, system/archive attributes, media,
disk-full and device errors remain unverified. Native entry closure does not
mean complete automatic CP/M-80 semantics. See [status](status.md).
