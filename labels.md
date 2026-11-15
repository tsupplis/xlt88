# Label / EQU state — CLOSED naming contract

```sh
./xlt88 --state-out=names.sym source.asm output.asm
# Edit preferred_name only.
./xlt88 --state-in=names.sym --state-out=names.sym source.asm output.asm
./xlt88 --state-in=names.sym --emit=asm80 source.asm review.asm
```

Version 2 JSON records original/preferred names, source-scoped IDs, definition
lines, EQU expressions/values and known source addresses. Version 1 imports.
Generated addresses remain null. Only preferred_name is editable; keep symbol
order and all metadata unchanged. Semantic changes belong in source.
Names use letters/digits/underscores, begin with letter/underscore and have
at least two characters. Registers/operators and case-insensitive duplicates
reject. Renaming never changes registers or string literals.

IDs are stable only for unchanged parsed source. Changed source/includes or
COM pointer interpretation require a fresh export. Automatic rebinding,
assembler-map import and arbitrary address annotations remain deferred.
COM-derived lxxxx labels use this same workflow.

Malformed/stale state and unsafe path aliases reject before output replacement.
Source/map/output/state paths must differ; state input/output may coincide.
Each file is staged separately: a second replacement failure can leave new
assembly with old state. State/layout round trips, byte equivalence, collisions,
path protection and focused ASan/UBSan checks passed.
