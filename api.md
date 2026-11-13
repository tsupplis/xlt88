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
| 15, 16, 19, 22, 23 | Open/close/delete/create/rename | Verify FCB fields and statuses | AH=0Fh, 10h, 13h, 16h, 17h, with native DOS FCBs |
| 17, 18 | Directory search | Verify search state and DMA directory entries | AH=11h–12h, consume native DOS search results |
| 20, 21 | Sequential read/write | Native calls demonstrated by hand-written copy | AH=14h–15h, including status 3 and DOS record layout |
| 24 | Login vector | Native word result to verify | Port drive enumeration explicitly |
| 25 | Current drive | Implemented | Implemented via AH=19h |
| 26 | Set DMA address | Implemented for the current DMA segment | Implemented via AH=1Ah (DTA) |
| 27, 31 | Allocation map, disk parameter block | Far pointer. Port consumers to use segment and offset explicitly | Port allocation and geometry consumers to the FAT interface |
| 28, 29 | Write-protect drive, read-only vector | Native calls to verify | Port write-protection policy explicitly |
| 30 | Set file attributes | Verify native FCB attribute bits | Port attribute operations explicitly |
| 32 | Get/set user area | Verify native user number semantics | Port user-area naming or directory policy explicitly |
| 33–36 | Random read/write, file size, set random record | Verify record fields and results | AH=21h–24h, adapt native DOS FCB record fields |
| 37 | Reset selected drives | Verify drive bitmap and results | Port selected-drive reset behavior explicitly |
| 38, 39 | Reserved in CP/M 2.2 | Reserved | Reserved source calls |
| 40 | Random write with zero fill | Verify native zero-fill behavior | Port zero filling explicitly. DOS AH=28h is block write |

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
