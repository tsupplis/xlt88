# Source readers

All source readers feed the same pipeline. Closed means the following contract,
not every historical assembler extension. Unsupported syntax must diagnose.
Text accepts LF/CRLF/CR, final lines without newline and Ctrl-Z EOF; NUL and
lines over 254 characters reject. Normalized output round-trips through Intel.

## Intel

Column-one labels or explicit colon labels; indented operations. Optional
initial ORG 100h. DB/DW/DS/EQU and standalone power-of-two ALIGN 1..256.
Direct transfer expressions bind known original instruction boundaries, not
byte offsets in expanded x86 code. Fixed origins reject target overflow.

## M80

Known operations may start in column one; .8080 and listing controls supported.
IF/IFT, IFE/IFF, IFDEF/IFNDEF, ELSE/ENDIF nest; conditions need previously known
values. INCLUDE/.INCLUDE resolve exact relative paths, with cycles rejected.
MACRO/ENDM supports positional/omitted/angle-grouped arguments, nested calls,
initial LOCAL labels and body-relative includes. REPT/IRP/IRPC and EXITM work;
EXITM exits the innermost expansion. Empty IRP/IRPC expand once with empty text.
Substitution supports &, ! escapes, % evaluation and IFB/IFNB/IFIDN/IFDIF.
String comparisons preserve case; double-semicolon macro comments disappear.

SET snapshots prior absolute values; cannot redefine EQU/labels. .RADIX uses
2..16 (default 10); B/D/H/O/Q suffixes override it. HIGH/LOW/NOT, arithmetic,
bitwise, shift and comparison operators are supported; true is 65535. Word
shifts bind with multiplication; << and >> retain their existing precedence.
NUL tests remaining text. Arithmetic uses checked host integers, not every
historical intermediate word-wrap rule. Ambiguous % output suffixes diagnose.

Limits: 32 include/macro nesting, 64 conditions/file, 32 parameters/locals/IRP
items, 65536 expansion iterations/records. No nested macro definitions,
parameter shadowing, IF1/IF2, Z80 or object modules. __m80_ is reserved.

## MAC

Adds optional line numbers, ! statement separators, embedded dollar removal,
16-character names, doubled quotes and selected listing controls. SET creates
versioned EQU bindings, including forward expressions. Labels on ORG/IF/END
retain location; END entry must resolve to 0100h. MACLIB loads one relative
basename.lib (basename ≤8). No library search path or pass-dependent assembly;
unsupported intermediate word wrapping in division/shifts/comparisons rejects.

## RMAC

Uses MAC expansion. Default CSEG; repeated CSEG/DSEG preserve section order,
then code precedes data at 0100h. Absolute-only ASEG uses common layout rules.
NAME is metadata; PUBLIC must resolve locally, with six-character collision
checks. Relocatable+absolute and matching-section subtraction are supported.
No REL linking, EXTRN/COMMON, phase directives, mixed absolute/relocatable
placement, labelled switches or ORG inside relocatable sections.

[COM input](com.md) is a separate annotated-binary reader, not yet closed.
