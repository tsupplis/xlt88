#include <ctype.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "symbols.h"

/* A bounded JSON reader for state files. Each token points past its complete
 * subtree, allowing object fields in arbitrary order. Unknown/duplicate fields
 * are rejected by the schema reader rather than silently changing bindings. */
typedef struct json_token {
	char kind;
	char text[256];
	size_t next;
} json_token_t;
typedef struct json_reader {
	FILE *input;
	int ch;
	json_token_t *tokens;
	size_t count, capacity;
	int failed;
} json_reader_t;

static void
advance(json_reader_t *reader)
{
	reader->ch = fgetc(reader->input);
}

static void
space(json_reader_t *reader)
{
	while (reader->ch == ' ' || reader->ch == '\n' || reader->ch == '\r' || reader->ch == '\t')
		advance(reader);
}

static int
hex_digit(int ch)
{
	if (ch >= '0' && ch <= '9') return ch - '0';
	if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
	if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
	return -1;
}

static void
parse_value(json_reader_t *reader, unsigned depth)
{
	size_t index = reader->count++, used = 0;
	int ch;
	if (depth > 16 || reader->count > 1000000) {
		reader->failed = 1;
		return;
	}
	if (reader->count > reader->capacity) {
		size_t capacity = reader->capacity ? reader->capacity * 2 : 64;
		void *tokens = realloc(reader->tokens, capacity * sizeof(*reader->tokens));
		if (!tokens) {
			reader->failed = 1;
			return;
		}
		reader->tokens = tokens;
		reader->capacity = capacity;
	}
	memset(&reader->tokens[index], 0, sizeof(reader->tokens[index]));
	space(reader);
	ch = reader->ch;
	reader->tokens[index].kind = (char)ch;
	advance(reader);
	if (ch == '{' || ch == '[') {
		int close = ch == '{' ? '}' : ']';
		space(reader);
		if (reader->ch != close) {
			for (;;) {
				if (ch == '{' && reader->ch != '"') goto invalid;
				parse_value(reader, depth + 1);
				if (reader->failed) return;
				space(reader);
				if (ch == '{') {
					if (reader->ch != ':') goto invalid;
					advance(reader);
					parse_value(reader, depth + 1);
					if (reader->failed) return;
					space(reader);
				}
				if (reader->ch == close) break;
				if (reader->ch != ',') goto invalid;
				advance(reader);
				space(reader);
			}
		}
		advance(reader);
	} else if (ch == '"') {
		while (reader->ch != '"') {
			ch = reader->ch;
			if (ch < 32 || ch == EOF) goto invalid;
			advance(reader);
			if (ch == '\\') {
				ch = reader->ch;
				advance(reader);
				switch (ch) {
				case '"': case '\\': case '/': break;
				case 'b': ch = '\b'; break;
				case 'f': ch = '\f'; break;
				case 'n': ch = '\n'; break;
				case 'r': ch = '\r'; break;
				case 't': ch = '\t'; break;
				case 'u': {
					int j, value = 0;
					for (j = 0; j < 4; ++j) {
						int digit = hex_digit(reader->ch);
						if (digit < 0) goto invalid;
						value = value * 16 + digit;
						advance(reader);
					}
					/* State identifiers and expressions retain source bytes. */
					if (!value || value > 255) goto invalid;
					ch = value;
					break;
				}
				default: goto invalid;
				}
			}
			if (used >= sizeof(reader->tokens[index].text) - 1) goto invalid;
			reader->tokens[index].text[used++] = (char)ch;
		}
		advance(reader);
	} else if (ch == 'n') {
		const char *tail = "ull";
		while (*tail) {
			if (reader->ch != *tail++) goto invalid;
			advance(reader);
		}
	} else if (ch == '-' || (ch >= '0' && ch <= '9')) {
		reader->tokens[index].kind = '0';
		reader->tokens[index].text[used++] = (char)ch;
		if (ch == '-') {
			ch = reader->ch;
			if (ch < '0' || ch > '9') goto invalid;
			reader->tokens[index].text[used++] = (char)ch;
			advance(reader);
		}
		while (reader->ch >= '0' && reader->ch <= '9') {
			if (ch == '0' || used >= 20) goto invalid;
			reader->tokens[index].text[used++] = (char)reader->ch;
			advance(reader);
		}
	} else goto invalid;
	reader->tokens[index].next = reader->count;
	return;
invalid:
	reader->failed = 1;
}

static int
fields(const json_token_t *tokens, size_t object, const char *const *names,
    size_t count, size_t *values)
{
	size_t i, j;
	if (tokens[object].kind != '{') return 0;
	memset(values, 0, count * sizeof(*values));
	for (i = object + 1; i < tokens[object].next;) {
		for (j = 0; j < count; ++j)
			if (!strcmp(tokens[i].text, names[j])) break;
		if (j == count || values[j]) return 0;
		values[j] = i + 1;
		i = tokens[i + 1].next;
	}
	for (j = 0; j < count; ++j)
		if (!values[j]) return 0;
	return 1;
}

static int
matches(const json_token_t *token, char kind, const char *text)
{
	return token->kind == kind && !strcmp(token->text, text);
}

static void
quote(FILE *out, const char *text)
{
	fputc('"', out);
	for (; *text; ++text) {
		unsigned char ch = (unsigned char)*text;
		if (ch == '"' || ch == '\\') fprintf(out, "\\%c", ch);
		else if (ch < 32 || ch >= 127) fprintf(out, "\\u%04x", ch);
		else fputc(ch, out);
	}
	fputc('"', out);
}

int
symbols_write(const symbol_table_t *table, FILE *out)
{
	size_t i;
	fprintf(out, "{\n  \"schema_version\": 2,\n  \"source_fingerprint\": \"%016" PRIx64 "\",\n  \"symbols\": [\n", table->fingerprint);
	for (i = 0; i < table->count; ++i) {
		const symbol_t *item = &table->items[i];
		long value = 0;
		int known = item->is_equ && symbols_eval(table, item->name, -1, &value);
		fprintf(out, "    {\"id\": %zu, \"original_name\": ", item->id);
		quote(out, item->name);
		fputs(", \"preferred_name\": ", out); quote(out, item->preferred);
		fprintf(out, ", \"kind\": \"%s\", \"definition\": %lu, \"equ_expression\": ", item->is_equ ? "equ" : "label", item->line);
		if (item->is_equ) quote(out, item->value); else fputs("null", out);
		fputs(", \"equ_value\": ", out);
		if (known) fprintf(out, "%ld", value); else fputs("null", out);
		fputs(", \"source_address\": ", out);
		if (item->source_address >= 0) fprintf(out, "%ld", item->source_address);
		else fputs("null", out);
		fprintf(out, ", \"generated_address\": null}%s\n", i + 1 < table->count ? "," : "");
	}
	fputs("  ]\n}\n", out);
	return !ferror(out);
}

int
symbols_read(symbol_table_t *table, FILE *input, FILE *diagnostics)
{
	static const char *const root_names[] = {"schema_version", "source_fingerprint", "symbols"};
	static const char *const entry_names[] = {"id", "original_name", "preferred_name", "kind", "definition", "equ_expression", "equ_value", "source_address", "generated_address"};
	json_reader_t reader = { .input = input };
	json_token_t *tokens;
	size_t root[3], entry[9], i, seen = 0;
	char fingerprint[32];
	symbol_table_t candidate = *table;
	int result = 0, legacy;
	candidate.items = calloc(table->count ? table->count : 1, sizeof(*table->items));
	if (!candidate.items) return 0;
	memcpy(candidate.items, table->items, table->count * sizeof(*table->items));
	advance(&reader);
	parse_value(&reader, 0);
	space(&reader);
	tokens = reader.tokens;
	if (reader.failed || reader.ch != EOF || ferror(input) ||
	    !fields(tokens, 0, root_names, 3, root)) goto invalid;
	snprintf(fingerprint, sizeof(fingerprint), "%016" PRIx64, table->fingerprint);
	legacy = matches(&tokens[root[0]], '0', "1");
	if ((!legacy && !matches(&tokens[root[0]], '0', "2")) ||
	    !matches(&tokens[root[1]], '"', fingerprint) || tokens[root[2]].kind != '[') goto invalid;
	for (i = root[2] + 1; i < tokens[root[2]].next; i = tokens[i].next) {
		symbol_t *item;
		char number[32];
		long value;
		int known;
		if (seen >= table->count || !fields(tokens, i, entry_names, 9, entry)) goto invalid;
		item = &candidate.items[seen++];
		snprintf(number, sizeof(number), "%zu", item->id);
		if (!matches(&tokens[entry[0]], '0', number) ||
		    !matches(&tokens[entry[1]], '"', item->name) || tokens[entry[2]].kind != '"' ||
		    !matches(&tokens[entry[3]], '"', item->is_equ ? "equ" : "label")) goto invalid;
		snprintf(number, sizeof(number), "%lu", item->line);
		if (!matches(&tokens[entry[4]], '0', number) ||
		    !matches(&tokens[entry[5]], item->is_equ ? '"' : 'n', item->is_equ ? item->value : "")) goto invalid;
		value = 0;
		if (legacy) {
			value = item->is_equ ? symbol_value(table, item->name) : -1;
			known = value >= 0;
		} else known = item->is_equ && symbols_eval(table, item->name, -1, &value);
		snprintf(number, sizeof(number), "%ld", value);
		if (!matches(&tokens[entry[6]], known ? '0' : 'n', known ? number : "") ||
		    tokens[entry[8]].kind != 'n') goto invalid;
		snprintf(number, sizeof(number), "%ld", item->source_address);
		known = !legacy && item->source_address >= 0;
		if (!matches(&tokens[entry[7]], known ? '0' : 'n', known ? number : "")) goto invalid;
		strcpy(item->preferred, tokens[entry[2]].text);
	}
	if (seen != table->count || !symbols_validate(&candidate, diagnostics)) goto invalid;
	memcpy(table->items, candidate.items, table->count * sizeof(*table->items));
	result = 1;
	goto done;
invalid:
	fputs("xlt88: invalid or stale symbol state; only preferred_name may be edited\n", diagnostics);
done:
	free(reader.tokens);
	free(candidate.items);
	return result;
}
