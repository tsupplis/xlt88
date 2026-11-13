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
	return 1;
}

void
symbols_destroy(symbol_table_t *table)
{
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

int
symbols_text(const symbol_table_t *table, const char *text, char *out, size_t capacity)
{
	size_t used = 0;
	while (*text) {
		const char *start = text++, *replacement = start;
		size_t length;
		char token[256];
		if (*start == '\'' || *start == '"') {
			while (*text && *text++ != *start)
				;
		} else if (isalnum((unsigned char)*start) || strchr("_.$?@#", *start)) {
			const symbol_t *item;
			while (*text && (isalnum((unsigned char)*text) || strchr("_.$?@#", *text)))
				++text;
			length = text - start;
			if (length >= sizeof(token))
				return 0;
			memcpy(token, start, length);
			token[length] = 0;
			item = symbol_find(table, token);
			if (item)
				replacement = item->preferred;
		}
		length = replacement == start ? (size_t)(text - start) : strlen(replacement);
		if (length >= capacity - used)
			return 0;
		memcpy(out + used, replacement, length);
		used += length;
	}
	out[used] = 0;
	return 1;
}
