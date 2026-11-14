/* RMAC single-module placement. Macro expansion precedes section grouping;
 * the resulting flat source still uses the common symbols/translator/emitter.
 * External linking, COMMON and mixed absolute/relocatable placement are explicit
 * boundaries, not directives silently erased to obtain plausible output. */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "symbols.h"

static int
raw_read(FILE *input, source_program_t *program)
{
	if (!mac_raw_read(input, program)) return 0;
	for (size_t i = 0; i < program->count; ++i) {
		source_record_t *record = &program->records[i];
		/* DRI's shipped RMAC modules also use $TITLE listing metadata. */
		if (record->problem && !strcmp(record->problem, "invalid MAC identifier") &&
		    record->text[0] == '$' && strlen(record->text) > 6 &&
		    tolower((unsigned char)record->text[1]) == 't' &&
		    tolower((unsigned char)record->text[2]) == 'i' &&
		    tolower((unsigned char)record->text[3]) == 't' &&
		    tolower((unsigned char)record->text[4]) == 'l' &&
		    tolower((unsigned char)record->text[5]) == 'e' &&
		    isspace((unsigned char)record->text[6]))
			record->problem = rmac_statement(record->text + 1, &record->statement);
		if (record->problem && !strcmp(record->problem, "M80 relocation/module directives are not implemented"))
			record->problem = rmac_statement(record->text, &record->statement);
	}
	return 1;
}

static void
directive(source_record_t *record)
{
	memset(&record->statement, 0, sizeof(record->statement));
	strcpy(record->statement.comment, "; RMAC module/section directive");
}

/* Relocation algebra follows RMAC: only adding an absolute displacement or
 * subtracting matching relocation types is legal. This pass checks types,
 * while the shared layout pass later evaluates the concrete placed values. */
static int expression_type(const source_program_t *, const unsigned char *,
    const expr_t *, size_t, unsigned);

static int
symbol_type(const source_program_t *source, const unsigned char *sections,
    const char *name, unsigned depth)
{
	for (size_t i = 0; i < source->count; ++i) {
		const statement_t *stmt = &source->records[i].statement;
		if (!symbol_equal(stmt->label, name)) continue;
		if (strcmp(stmt->opcode, "equ")) return sections[i];
		const char *error;
		expr_t *tree = expr_parse(stmt->first, NULL, NULL, 0, &error);
		const expr_t *value = tree;
		/* The shared MAC preprocessor wraps EQU/SET in a word-size mask. */
		if (tree && tree->kind == expr_binary && tree->op == expr_and &&
		    tree->right->kind == expr_integer && tree->right->integer == 65535)
			value = tree->left;
		int type = expression_type(source, sections, value, i, depth + 1);
		expr_destroy(tree);
		return type;
	}
	return -1;
}

static int
expression_type(const source_program_t *source, const unsigned char *sections,
    const expr_t *tree, size_t record, unsigned depth)
{
	int left, right;
	if (!tree || depth > 64) return -1;
	if (tree->kind == expr_integer || tree->kind == expr_string) return 0;
	if (tree->kind == expr_location) return sections[record];
	if (tree->kind == expr_symbol) return symbol_type(source, sections, tree->text, depth + 1);
	left = expression_type(source, sections, tree->left, record, depth + 1);
	if (left < 0) return -1;
	if (tree->kind == expr_unary) return left == 0 ? 0 : -1;
	right = expression_type(source, sections, tree->right, record, depth + 1);
	if (right < 0) return -1;
	if (tree->kind == expr_list) return left || right ? 1 : 0;
	if (tree->op == expr_plus) return left && right ? -1 : left + right;
	if (tree->op == expr_minus) return !right ? left : left == right ? 0 : -1;
	return !left && !right ? 0 : -1;
}

static int
check_expression(const source_program_t *source, const unsigned char *sections,
    const char *text, size_t record, int absolute)
{
	const char *error;
	expr_t *tree = expr_parse(text, NULL, NULL, 1, &error);
	int type = expression_type(source, sections, tree, record, 0);
	expr_destroy(tree);
	return type >= 0 && (!absolute || !type);
}

static const char *
public_names(const source_program_t *source, const unsigned char *sections,
    const statement_t *stmt, char names[1024][17], size_t *count)
{
	const char *fields[] = {stmt->first, stmt->second};
	if (stmt->label[0] || !stmt->first[0]) return "invalid RMAC PUBLIC directive";
	for (size_t f = 0; f < 2; ++f) {
		const char *p = fields[f];
		while (*p) {
			char name[17];
			size_t n = 0;
			while (isspace((unsigned char)*p)) ++p;
			while (isalnum((unsigned char)*p) || *p == '?' || *p == '@') {
				if (n == 16) return "RMAC PUBLIC symbol exceeds 16 characters";
				name[n++] = (char)tolower((unsigned char)*p++);
			}
			name[n] = 0;
			while (isspace((unsigned char)*p)) ++p;
			if (!n || isdigit((unsigned char)name[0]) || (*p && *p != ','))
				return "invalid RMAC PUBLIC symbol list";
			if (symbol_type(source, sections, name, 0) < 0)
				return "RMAC PUBLIC symbol must be defined in this module";
			for (size_t k = 0; k < *count; ++k)
				if (!strncmp(names[k], name, 6) && strcmp(names[k], name))
					return "RMAC PUBLIC names collide in their first six characters";
			if (*count == 1024) return "RMAC PUBLIC declaration limit exceeded";
			strcpy(names[(*count)++], name);
			if (!*p) break;
			++p;
			if (!*p) return "empty RMAC PUBLIC symbol";
		}
	}
	return NULL;
}

static int
place(source_program_t *program)
{
	unsigned char *sections = calloc(program->count ? program->count : 1, 1);
	source_record_t *ordered = NULL;
	int section = 1, absolute = 0, relocatable = 0;
	size_t end = program->count, used = 0;
	char public_symbols[1024][17];
	size_t public_count = 0;
	if (!sections) return 0;
	for (size_t i = 0; i < program->count; ++i) {
		source_record_t *record = &program->records[i];
		statement_t *stmt = &record->statement;
		const char *op = stmt->opcode;
		sections[i] = section;
		if (record->problem) goto rejected;
		if (!strcmp(op, "aseg") || !strcmp(op, "cseg") || !strcmp(op, "dseg")) {
			if (stmt->label[0] || stmt->first[0] || stmt->second[0]) {
				record->problem = "RMAC section labels/operands require explicit placement"; goto rejected;
			}
			section = !strcmp(op, "aseg") ? 0 : !strcmp(op, "cseg") ? 1 : 2;
			sections[i] = section;
			directive(record);
			continue;
		}
		if (!strcmp(op, "common") || !strcmp(op, "extrn") || !strcmp(op, "entry") ||
		    !strcmp(op, ".phase") || !strcmp(op, ".dephase")) {
			record->problem = "RMAC external/common/phased modules require linker support"; goto rejected;
		}
		if (!strcmp(op, "public") || !strcmp(op, "name")) continue;
		if (!strcmp(op, "end")) { end = i; continue; }
		if (!*op || !strcmp(op, "equ")) continue;
		if (!section) absolute = 1;
		else relocatable = 1;
		if (absolute && relocatable) {
			record->problem = "RMAC mixed absolute and relocatable sections require explicit link placement"; goto rejected;
		}
		if (section && !strcmp(op, "org")) {
			record->problem = "RMAC ORG in a relocatable section requires linker placement; use ASEG for absolute input"; goto rejected;
		}
	}
	for (size_t i = 0; i < program->count; ++i) {
		source_record_t *record = &program->records[i];
		statement_t *stmt = &record->statement;
		const char *op = stmt->opcode;
		if (!strcmp(op, "name")) {
			/* NAME is metadata for a single module; no object export is claimed. */
			if (stmt->label[0] || !stmt->first[0] || stmt->second[0]) {
				record->problem = "invalid RMAC NAME directive"; goto rejected;
			}
			directive(record); continue;
		}
		if (!strcmp(op, "public")) {
			record->problem = public_names(program, sections, stmt, public_symbols, &public_count);
			if (record->problem) goto rejected;
			directive(record); continue;
		}
		if (!relocatable || !*op) continue;
		if (!strcmp(op, "equ")) {
			if (symbol_type(program, sections, stmt->label, 0) >= 0) continue;
		} else if (!strcmp(op, "lxi")) {
			if (check_expression(program, sections, stmt->second, i, 0)) continue;
		} else if (!strcmp(op, "mvi")) {
			if (check_expression(program, sections, stmt->second, i, 1)) continue;
		} else if (!strcmp(op, "adi") || !strcmp(op, "aci") || !strcmp(op, "sui") ||
		    !strcmp(op, "sbi") || !strcmp(op, "ani") || !strcmp(op, "xri") ||
		    !strcmp(op, "ori") || !strcmp(op, "cpi") || !strcmp(op, "in") ||
		    !strcmp(op, "out") || !strcmp(op, "rst")) {
			if (check_expression(program, sections, stmt->first, i, 1)) continue;
		} else if (!strcmp(op, "db") || !strcmp(op, "dw") || !strcmp(op, "ds") ||
		    !strcmp(op, "org") || !strcmp(op, "align") || !strcmp(op, "end") ||
		    !strcmp(op, "lda") || !strcmp(op, "sta") || !strcmp(op, "lhld") || !strcmp(op, "shld") ||
		    op[0] == 'j' || (op[0] == 'c' && strcmp(op, "cmp") && strcmp(op, "cma") && strcmp(op, "cmc") && strcmp(op, "cpi"))) {
			if (!stmt->first[0] || check_expression(program, sections, stmt->first, i,
			    !strcmp(op, "db") || !strcmp(op, "ds") || !strcmp(op, "align") || !strcmp(op, "org"))) continue;
		} else continue;
		record->problem = "invalid or unresolved RMAC relocation expression"; goto rejected;
	}
	if (!relocatable) { free(sections); return 1; }
	ordered = calloc(program->count ? program->count : 1, sizeof(*ordered));
	if (!ordered) { free(sections); return 0; }
	/* Flatten this one module as code followed by data; retain source provenance
	 * and preserve each section's order across repeated section switches. */
	for (int group = 0; group <= 2; ++group)
		for (size_t i = 0; i < program->count; ++i)
			if (i != end && sections[i] == group) ordered[used++] = program->records[i];
	if (end < program->count) ordered[used++] = program->records[end];
	for (size_t i = 0; i < used; ++i) ordered[i].line = i + 1;
	free(program->records);
	program->records = ordered;
	free(sections);
	return 1;
rejected:
	free(sections);
	return 1; /* Source diagnostics are owned by the ordinary pipeline. */
}

int
rmac_read_file(FILE *input, const char *path, source_program_t *program)
{
	return macro_read_file(input, path, program, raw_read, macro_rmac) && place(program);
}

static int
read_rmac(FILE *input, source_program_t *program)
{
	return rmac_read_file(input, NULL, program);
}

const dialect_t rmac_dialect = { "rmac", read_rmac };
