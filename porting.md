# Reviewing a translated program

xlt88 translates native code and native OS operations. A successful translation
or assembly does not establish full program equivalence. This checklist records
where explicit post-translation adjustment may be appropriate. General analysis
must depend on source facts, not application names or copies of reference patches.

## CP/M-86 versus DOS

CP/M-86 is usually the closer target: its BDOS services and file-control model
are related to CP/M-80. DOS adds FCB representation differences, different disk
structures and missing CP/M concepts such as user areas and IOBYTE routing.
Neither target preserves the original flat address space automatically. CPU
translation expansion, segmented addresses, interrupts and hardware dependencies
can require substantial work even on CP/M-86. Similar API numbers do not prove
identical parameters, results or state changes; see api.md for each contract.

## What to inspect

| Source pattern | Why it matters | Current evidence or action |
| --- | --- | --- |
| Fixed ORG, reserved gaps and alignment directives | Expanded instructions can overlap fixed data; moving data may violate an algorithm | The region planner retains fixed origins. Inspect assembler output/maps for placement and available space; do not solve overflow by moving a block without checking its users. |
| Low-byte-only pointer arithmetic, fixed high bytes, masks such as FF00h, HIGH/LOW or shifts | Implicit 256-byte page assumptions may be hidden in arithmetic | Inspect the whole address computation and its maximum index. VTL-2's `convp` drops the carry; its variables remain at 0800h. No general proof of implicit alignment is claimed. |
| Address tables, numeric pointers, stored return addresses and code-label arithmetic | Source instruction sizes change; numeric offsets may no longer identify the intended instruction or byte | Symbol binding handles some known source boundaries. Inspect unresolved offsets and consumers; do not equate successful assembly with correct relocation. |
| Inline data following CALL, return-address modification or self-modifying code | The source depends on the actual code/data layout or instruction encoding | Test the affected execution paths and inspect the generated instructions. WASH's five scenarios exercise inline messages; this is not proof for every such program. |
| BDOS pointer results and direct OS-memory access | A returned offset alone can lose a CP/M-86 segment; DOS structures differ further | Functions 27/31 now emit ES:BX with review diagnostics; their consumers remain explicit port work. DISK7's supplied reference uses ES for returned system-data pointers. Follow pointer copies and dereferences, not just the original call. |
| SP changes, alternate stacks, DI/EI and interrupt handlers | Stack segment and interrupt policy are part of the target execution contract | Review SS:SP setup and interrupt restoration. WASH's reference adds STI, but its source DI policy is preserved by translation. Emulator transcripts do not establish hardware interrupt behavior. |
| FCB field accesses, numeric default-FCB addresses and object copies | Layout, metadata and size differences can invalidate otherwise correct file calls | Use the documented DOS FCB analysis limits. First-default native storage has a defined boundary; private storage and incompatible field access need separate audits. |
| Memory-loaded BDOS selectors and indirect writes | Known stores may suggest services without proving that all writes are known | Dedicated DS 1 candidates get runtime checks. Unmatched CP/M-86 selectors pass through natively with a review warning; DOS terminates. This is bounded support, not general dynamic BDOS compatibility. |
| BIOS/ROM entry points, port I/O, display memory and disk geometry assumptions | These describe a machine or OS implementation, not portable application behavior | Provide an explicit native port and suitable hardware tests. Do not infer replacements from the application's identity. |
| TPA limits, heap/stack growth, pointer wrap and addresses near FFFFh | A combined image may assemble while runtime allocation or arithmetic exceeds its valid region | Inspect target allocation and worst-case growth; exercise large input and boundary cases. Segmentation and overflow need their own design, not an automatic alignment fix. |

The 8086/8088 permits unaligned word accesses. Odd addresses therefore do not
in themselves require padding. The difficult alignment problems here are
algorithmic page assumptions, fixed layout and segment/load contracts. Adding
padding indiscriminately can itself break address arithmetic or data formats.

Explicit constraints are easier to detect than implicit ones, but are not a
complete specification: an ORG gives an address without explaining why it is
required. Page assumptions may be spread across EQU expressions, macros,
register operations, table construction and dynamically computed indices.
Record a required alignment and its reason when manual inspection establishes
one; a general alignment-constraint inference system is not yet implemented.

## Review and adjustment workflow

1. Preserve the original source and record the translation command, diagnostics,
   target OS, assembler and tool versions. Keep any deliberate port separate.
2. Resolve diagnostics by identifying the source contract. When a generic rule
   can be justified, add independent positive and negative fixtures, including
   renamed symbols where relevant. Otherwise document the manual adjustment.
3. Inspect the generated listing/map for origins, gaps, table locations, stack
   space and segment assumptions. Check arithmetic bounds, not just addresses.
4. Compare observable behavior with a trusted reference: results, files, error
   paths, boundary inputs and repeated calls. State which paths were exercised.
5. Validate device, interrupt, user-area and protection behavior on a runner
   that actually implements it. Record unsupported emulator behavior as
   unverified rather than adding an emulator-specific translator workaround.

For each manual adjustment, record the original assumption, the target
requirement, the exact change and its validation. An intentional policy change
(such as enabling interrupts) must remain distinguishable from a translation
correction. A patched reference is a useful acceptance target, not automatic
permission to apply all of its patches to unrelated programs.

Current measurements and limitations: progress.md, api.md, refs.md, wash.md and
vtlbench.md. The corpus inventory only executes its stated smoke fixtures;
VTL-2 and WASH execution evidence comes from their separate acceptance suites.


## Comparing with known-working target references

Use both independent contract tests and application reference comparisons.
For an executable translation, compare complete console transcripts and resulting
file contents from identical initial fixtures, including failure and boundary
paths. Keep fixtures isolated from generated binaries when disk free-space
output depends on directory contents. Preserve source and reference hashes.

Inspect assembly differences to explain segment overrides, stack setup,
interrupt policy and layout; identical assembly is not a goal. Track binary
size separately, noting container/padding differences. WASH's report does both
behavior and size comparisons. DISK7 currently has reference-only acceptance
and an explicit unchanged-source blocker inventory; it has no generated-program
behavior comparison yet. A working patched reference does not prove automatic
translation or justify importing its patches as heuristics.

WASH's explicit allocation example is in `tests/washport.py` and `washport.md`.
The test-only changes enable interrupts after stack selection, read the native
group limit, reserve a copy record while building the directory, and calculate
buffer capacity by subtraction and division. They do not modify the original
source or introduce application-specific translator rules. The native group
descriptor contains an inclusive last-byte offset; this example deliberately
uses it as a conservative exclusive limit, leaving one byte unused.

emu2 currently grants these 8080-model CMDs at least 64 KiB. Smaller limits in
this test are explicit base-page overrides with canary checks, not evidence
that CMD min/max requests receive those allocations. CMD sizing and allocation
policy require a separate design decision and are deferred.
