# Digital Research RMAC source reader

Select `--dialect=rmac`; `--emit=8080` exposes the normalized flat source.
This is a single-module reader/placement milestone, not a REL-object linker
or a claim of complete RMAC compatibility.

The syntax/relocation reference is Digital Research's
[LINK-80 Operator's Guide, RMAC chapter](https://www.bitsavers.org/pdf/digitalResearch/cpm/LINK-80_Operators_Guide_Dec80.pdf).

| Area | Current contract |
| --- | --- |
| Lexical/macros | Shared MAC lexical rules, conditional assembly, macro/repeat expansion and versioned SET |
| Libraries | MACLIB basename loads the relative `basename.lib`; eight-character basename, one library per directive; normal INCLUDE also works |
| CSEG / DSEG | Default CSEG; repeated switches preserve order within each section. One module is placed as code followed by data at the normal 0100h program origin |
| ASEG | Absolute-only modules use the existing ORG/layout rules |
| NAME | Single-module metadata; no object-module export is produced |
| PUBLIC | Locally defined symbol lists; first-six-character collisions rejected; no cross-module resolution |
| Expressions | Relocatable plus absolute and matching-section subtraction; other relocation arithmetic rejected. Existing MAC intermediate-word-arithmetic restrictions remain |
| Provenance/state | Physical input/library locations survive section grouping; shared editable name/EQU state round-trips |
| Explicit limitations | EXTRN, COMMON, phased modules, mixed absolute/relocatable placement, labelled section switches and ORG inside a relocatable section require further work |

Only assembler source is read; `.REL` files are not inputs. The reader does not
guess external definitions or silently discard unresolved module dependencies.
Code/data are sections of one flat translated program, not inferred x86 segments.
The program must provide its entry at the start of the placed code; END entry
validation remains in the common translation pipeline.

`tests/rmac.py` executes relocatable and absolute fixtures on both targets through
all four emitters. It checks libraries/macros, repeated section switches,
normalization/state round trips, relocation-type errors, provenance and atomic
output rejection. The unchanged DRI CP/M 3 filename parser in `tests/parse.asm` also executes
inside a source-include harness on both targets through all four emitters.
Ordinary names, wildcards, passwords and an invalid drive are checked with a
storage guard. This is legacy-source acceptance, not object-module linking.
Full linking and a broader external RMAC corpus remain open.

The parser fixture is copied byte-for-byte from the local CP/M 3 source bundle
`CPM/cpm3-agn453/cpm3src_unix/parse.asm` (DRI Filename Parser).
SHA256: `0dcec1a69d079e22d83dec34778b4d600e72088102a60429354c5e9c764fe579`.
