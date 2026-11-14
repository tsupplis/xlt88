# WASH15 DOS port analysis

This is a blocker analysis, not a DOS implementation or execution claim. The
original source and supplied reference were not changed. DOS remains strict
when service selection or FCB representation cannot be established.

## Evidence in the current source

| Area | Observed source behavior | Work required |
| --- | --- | --- |
| Object declarations | SORCFCB and DESTFCB each have a standalone label followed by DS 33 | Resolved generically: DS discovery crosses blank/comment records while retaining alias and extent checks. Unchanged WASH now reaches the subroutine-effects blocker below. |
| Subroutine effects | MOVBYT handles several copy widths and source/destination pairs, including a 33-byte FCB copy | The current audit only proves a narrow name-copy pattern. A temporary declaration-only normalization reaches `unrecognized subroutine may expose FCB storage`. Parameterized copy effects and per-call address/width evidence are needed before relaxing this check. |
| FCB contents | Explicit accesses initialize EX/S1/S2/RC and current-record fields; rename uses a second name at +16 | Classify initialization, file-state access and rename records separately. Growing DS 33 to DS 36 alone does not prove that all copied metadata has native DOS meaning. |
| Directory results | NM2LST masks the result with 3, multiplies by 32, indexes BUFR and copies a 12-byte name prefix | DOS FCB search returns a different record representation. Functions 17/18 need an explicit result-conversion design or a source port, not just native call-number substitution. |
| Console buffer | Rename uses BDOS 10 with a CP/M buffer length | DOS lowering now adapts capacities 0..254 and keeps its CR outside caller storage; editing/completion remain native DOS policy. WASH cannot execute on DOS until the remaining FCB/search blockers are resolved. |
| Memory allocation | Copy buffer sizing reads BDOS+1, then grows a 16-bit pointer in 128-byte steps | This CP/M-80 memory-limit assumption is not a native DOS allocation contract. It also limits the supplied CP/M-86 reference. Keep a native allocation-aware source/target adjustment explicit. |

The earlier declaration-only experiment is now superseded by shared storage
discovery. No WASH source rewriting is required to clear that declaration
blocker; this does not establish whole-program DOS acceptance.

## Suggested implementation order

1. Declaration discovery is implemented and tested; preserve its conservative
   alias/extent rules while extending the subsequent use analysis.
2. Define the DOS search-result contract and console-buffer translation.
3. Extend copy-effect analysis only for demonstrably bounded source patterns.
   Audit each FCB field access; keep unsupported cases diagnostic.
4. Decide and document the native allocation port before testing successful
   WASH copies across memory sizes. Compare file content and failure behavior.

No WASH-specific symbol recognition, synthetic CP/M address space or hidden
FCB compatibility runtime is proposed. Passing the first audit must not be
mistaken for removing all the later blockers.
