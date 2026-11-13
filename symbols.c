#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "symbols.h"

int
symbol_equal(const char *a, const char *b)
{
	while (*a && tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
		++a;
		++b;
	}
	return *a == *b;
}

long
symbol_number(const char *text)
{
	char *end;
	long result;
	size_t length = strlen(text);
	int hex = length && tolower((unsigned char)text[length - 1]) == 'h';
	if (!isdigit((unsigned char)*text))
		return -1;
	errno = 0;
	result = strtol(text, &end, hex || (text[0] == '0' &&
	    tolower((unsigned char)text[1]) == 'x') ? 16 : 10);
	return !errno && end == text + length - hex ? result : -1;
}

const symbol_t *
symbol_find(const symbol_table_t *table, const char *name)
{
	size_t i;
	for (i = 0; i < table->count; ++i)
		if (symbol_equal(table->items[i].name, name))
			return &table->items[i];
	return NULL;
}

const char *
symbol_alias(const symbol_table_t *table, const char *text)
{
	size_t depth;
	for (depth = 0; depth <= table->count; ++depth) {
		const symbol_t *item = symbol_find(table, text);
		if (!item || !item->is_equ)
			return text;
		text = item->value;
	}
	return NULL;
}

long
symbol_value(const symbol_table_t *table, const char *text)
{
	const char *alias = symbol_alias(table, text);
	return alias ? symbol_number(alias) : -1;
}

static void
fingerprint_text(symbol_table_t *table, const char *text)
{
	/* FNV-1a is an identity check, not a cryptographic signature. Include a
	 * field separator so different statement fields cannot concatenate. */
	do {
		table->fingerprint ^= (unsigned char)*text;
		table->fingerprint *= UINT64_C(1099511628211);
	} while (*text++);
}

int
symbols_build(const source_program_t *source, symbol_table_t *table, FILE *diagnostics)
{
	size_t i;
	table->fingerprint = UINT64_C(14695981039346656037);
	table->items = calloc(source->count ? source->count : 1, sizeof(*table->items));
	if (!table->items)
		return 0;
	for (i = 0; i < source->count; ++i) {
		const source_record_t *record = &source->records[i];
		const statement_t *stmt = &record->statement;
		symbol_t *item;
		fingerprint_text(table, stmt->label);
		fingerprint_text(table, stmt->opcode);
		fingerprint_text(table, stmt->first);
		fingerprint_text(table, stmt->second);
		fingerprint_text(table, stmt->comment);
		if (record->problem) {
			fprintf(diagnostics, "xlt88:%lu: %s\n", record->line, record->problem);
			return 0;
		}
		if (!strcmp(stmt->opcode, "end"))
			break;
		if (!stmt->label[0])
			continue;
		if (symbol_find(table, stmt->label)) {
			fprintf(diagnostics, "xlt88:%lu: duplicate source symbol: %s\n", record->line, stmt->label);
			return 0;
		}
		item = &table->items[table->count++];
		item->id = table->count;
		strcpy(item->name, stmt->label);
		strcpy(item->preferred, stmt->label);
		strcpy(item->value, stmt->first);
		item->is_equ = !strcmp(stmt->opcode, "equ");
		item->line = record->line;
	}
	return symbols_layout(source, table, diagnostics);
}

void
symbols_destroy(symbol_table_t *table)
{
	size_t i;
	for (i = 0; i < table->count; ++i) expr_destroy(table->items[i].expression);
	free(table->items);
	memset(table, 0, sizeof(*table));
}

int
symbols_validate(const symbol_table_t *table, FILE *diagnostics)
{
	size_t i, j;
	for (i = 0; i < table->count; ++i) {
		const char *name = table->items[i].preferred;
		/* Keep renamed source identifiers unambiguous in every operand
		 * position and avoid generated L@ labels and Intel operators. */
		if (strcmp(name, table->items[i].name)) {
			if ((!isalpha((unsigned char)*name) && *name != '_') || strlen(name) < 2 ||
			    symbol_equal(name, "sp") || symbol_equal(name, "psw") ||
			    symbol_equal(name, "and") || symbol_equal(name, "or") ||
			    symbol_equal(name, "xor") || symbol_equal(name, "not") ||
			    symbol_equal(name, "mod") || symbol_equal(name, "shl") || symbol_equal(name, "shr"))
				goto invalid;
			for (j = 1; name[j]; ++j)
				if (!isalnum((unsigned char)name[j]) && name[j] != '_')
					goto invalid;
		}
		for (j = 0; j < i; ++j)
			if (symbol_equal(name, table->items[j].preferred))
				goto invalid;
	}
	return 1;
invalid:
	fprintf(diagnostics, "xlt88: invalid or duplicate preferred symbol name: %s\n", table->items[i].preferred);
	return 0;
}

static const char *
preferred_name(void *context, const char *name, int *equ, size_t *id)
{
	const symbol_t *item = symbol_find(context, name);
	if (!item) return NULL;
	*equ = item->is_equ;
	*id = item->id;
	return item->preferred;
}

typedef struct text_buffer {
	char *text;
	size_t capacity, used;
	int failed;
} text_buffer_t;

static void
append(text_buffer_t *out, const char *text)
{
	size_t length = strlen(text);
	if (out->failed) return;
	if (length >= out->capacity - out->used) { out->failed = 1; return; }
	memcpy(out->text + out->used, text, length);
	out->used += length;
	out->text[out->used] = 0;
}

static void
source_expression(text_buffer_t *out, const expr_t *tree)
{
	static const char *const operators[] = {"+", "-", "*", "/", "%", " AND ", "|", "^", "<<", ">>", "~"};
	switch (tree->kind) {
	case expr_integer: case expr_string: case expr_symbol: case expr_location:
		append(out, tree->text);
		break;
	case expr_unary:
		append(out, "("); append(out, operators[tree->op]);
		source_expression(out, tree->left); append(out, ")");
		break;
	case expr_binary:
		append(out, "("); source_expression(out, tree->left);
		append(out, operators[tree->op]); source_expression(out, tree->right);
		append(out, ")");
		break;
	case expr_list:
		source_expression(out, tree->left); append(out, ",");
		source_expression(out, tree->right);
		break;
	}
}

int
symbols_text(const symbol_table_t *table, const char *text, char *out, size_t capacity)
{
	const char *error;
	text_buffer_t buffer = { out, capacity, 0, 0 };
	expr_t *tree = expr_parse(text, preferred_name, (void *)table, 1, &error);
	if (!tree || !capacity) { expr_destroy(tree); return 0; }
	source_expression(&buffer, tree);
	expr_destroy(tree);
	return !buffer.failed;
}
