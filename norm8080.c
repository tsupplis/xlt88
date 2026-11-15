#include <string.h>

#include "symbols.h"

/* Only expression fields participate in renaming. Keeping the complete list
 * here avoids treating registers, literals or mnemonic prefixes as symbols. */
static int
expression_field(const char *opcode)
{
	static const char *const first[] = {
		"equ", "db", "dw", "ds", "org", "align", "lda", "sta", "lhld", "shld",
		"adi", "aci", "sui", "sbi", "ani", "xri", "ori", "cpi", "in", "out", "rst",
		"jmp", "jnz", "jz", "jnc", "jc", "jpo", "jpe", "jp", "jm",
		"call", "cnz", "cz", "cnc", "cc", "cpo", "cpe", "cp", "cm"
	};
	if (!strcmp(opcode, "lxi") || !strcmp(opcode, "mvi")) return 2;
	for (size_t i = 0; i < sizeof(first) / sizeof(*first); ++i)
		if (!strcmp(opcode, first[i])) return 1;
	return 0;
}

int
emit_named_8080(const source_program_t *program, const symbol_table_t *symbols, FILE *out, FILE *diagnostics)
{
	size_t i;

	for (i = 0; i < program->count; ++i) {
		const source_record_t *record = &program->records[i];
		statement_t renamed = record->statement;
		const statement_t *stmt = &renamed;
		const symbol_t *label;
		if (record->problem) {
			source_report(diagnostics, record, record->problem);
			return 0;
		}
		if (symbols) {
			label = symbol_find(symbols, stmt->label);
			if (label) strcpy(renamed.label, label->preferred);
			switch (expression_field(stmt->opcode)) {
			case 1:
				if (!symbols_text(symbols, record->statement.first, renamed.first, sizeof(renamed.first))) goto too_long;
				break;
			case 2:
				if (!symbols_text(symbols, record->statement.second, renamed.second, sizeof(renamed.second))) goto too_long;
				break;
			default:
				break;
			}
		}
		/* The Intel reader accepts at most 254 content bytes per line. */
		if (strlen(stmt->label) + (stmt->label[0] && strcmp(stmt->opcode, "equ") ? 1 : 0) +
		    (stmt->opcode[0] ? 1 + strlen(stmt->opcode) : 0) +
		    (stmt->first[0] ? 1 + strlen(stmt->first) : 0) +
		    (stmt->second[0] ? 1 + strlen(stmt->second) : 0) +
		    (stmt->comment[0] ? 1 + strlen(stmt->comment) : 0) > 254)
			goto too_long;
		if (stmt->label[0])
			fprintf(out, "%s%s", stmt->label, !strcmp(stmt->opcode, "equ") ? "" : ":");
		if (stmt->opcode[0]) {
			fprintf(out, "\t%s", stmt->opcode);
			if (stmt->first[0])
				fprintf(out, "\t%s", stmt->first);
			if (stmt->second[0])
				fprintf(out, ",%s", stmt->second);
		}
		if (stmt->comment[0])
			fprintf(out, "\t%s", stmt->comment);
		fputs("\r\n", out);
	}
	/* Normalized source is also input for native CP/M assemblers. */
	fputc(0x1a, out);
	return !ferror(out);
too_long:
	fputs("xlt88: invalid normalized expression or source line/field limit exceeded\n", diagnostics);
	return 0;
}

int
emit_normalized_8080(const source_program_t *program, FILE *out, FILE *diagnostics)
{
	return emit_named_8080(program, NULL, out, diagnostics);
}
