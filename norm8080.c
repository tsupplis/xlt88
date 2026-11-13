#include <string.h>

#include "symbols.h"

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
			fprintf(diagnostics, "xlt88:%lu: %s\n", record->line, record->problem);
			return 0;
		}
		if (symbols) {
			label = symbol_find(symbols, stmt->label);
			if (label) strcpy(renamed.label, label->preferred);
			/* Only expression operands are renamed: registers and strings
			 * must keep their meaning even when a source label is A or B. */
			if (!strcmp(stmt->opcode, "lxi") || !strcmp(stmt->opcode, "mvi")) {
				if (!symbols_text(symbols, record->statement.second, renamed.second, sizeof(renamed.second))) goto too_long;
			} else if (!strcmp(stmt->opcode, "equ") || !strcmp(stmt->opcode, "db") ||
			    !strcmp(stmt->opcode, "dw") || !strcmp(stmt->opcode, "ds") ||
			    !strcmp(stmt->opcode, "org") || !strcmp(stmt->opcode, "align") ||
			    !strcmp(stmt->opcode, "lda") || !strcmp(stmt->opcode, "sta") ||
			    !strcmp(stmt->opcode, "lhld") || !strcmp(stmt->opcode, "shld") ||
			    !strcmp(stmt->opcode, "adi") || !strcmp(stmt->opcode, "aci") ||
			    !strcmp(stmt->opcode, "sui") || !strcmp(stmt->opcode, "sbi") ||
			    !strcmp(stmt->opcode, "ani") || !strcmp(stmt->opcode, "xri") ||
			    !strcmp(stmt->opcode, "ori") || !strcmp(stmt->opcode, "cpi") ||
			    !strcmp(stmt->opcode, "in") || !strcmp(stmt->opcode, "out") ||
			    !strcmp(stmt->opcode, "rst") || stmt->opcode[0] == 'j' ||
			    (!strncmp(stmt->opcode, "c", 1) && strcmp(stmt->opcode, "cmp") &&
			    strcmp(stmt->opcode, "cma") && strcmp(stmt->opcode, "cmc"))) {
				if (!symbols_text(symbols, record->statement.first, renamed.first, sizeof(renamed.first))) goto too_long;
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
		fputc('\n', out);
	}
	return !ferror(out);
too_long:
	fputs("xlt88: normalized source exceeds source line or field limit\n", diagnostics);
	return 0;
}

int
emit_normalized_8080(const source_program_t *program, FILE *out, FILE *diagnostics)
{
	return emit_named_8080(program, NULL, out, diagnostics);
}
