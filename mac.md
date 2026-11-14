# Digital Research MAC reader

Select `--dialect=mac`; normalize independently with `--emit=8080`.
The reader targets a flat 8080 subset of Digital Research MAC, using the
[MAC manual](https://bitsavers.org/pdf/digitalResearch/cpm/CPM_MAC_Macro_Assembler_Nov80.pdf)
as the syntax reference. It is not an alias for M80 and does not implement RMAC.

| Area | Implemented contract |
| --- | --- |
| Lexical input | CR/LF, CP/M EOF, optional line numbers, `!` statement separators outside literals/comments |
| Names | Case-insensitive, up to 16 significant characters; embedded dollar separators removed outside strings |
| Literals | Dollar-separated numbers; doubled quotes preserved as exact byte expressions |
| Preprocessing | Shared macro/conditional engine, nested IF/ELSE/ENDIF, selected 8080 macro branches |
| SET | Each assignment creates a private EQU version; earlier uses and forward expressions retain their binding |
| Directives | Labels on ORG, IF and END retain their location; END entry is preserved and must resolve to 0100h |
| Diagnostics | Original physical line provenance; errors prevent replacement of existing output |

Records are limited to 254 characters. Listing controls supported by the reader
are discarded; unknown controls are not silently accepted. Complete MACLIB,
relocatable modules, pass-dependent conditions and full intermediate 16-bit
word arithmetic are not claimed. EQU/SET values wrap to 16 bits; expressions
whose division, shifts or comparisons require unsupported intermediate wrapping
are conservatively rejected. Shared M80 extensions are not proof of full MAC
compatibility.

`tests/mac.py` checks exact data bytes and native execution through all four
assemblers on DOS and CP/M-86, plus rejection/atomic-output checks and unchanged
DISK7 normalization. DISK7 normalization passes; its native translation and
execution remain blocked by BDOS analysis/lowering limitations.
