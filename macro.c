#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "macro.h"
#include "symbols.h"

static int
word(int ch)
{
	return ch && (isalnum((unsigned char)ch) || strchr("_.$?@#", ch));
}

static int
identifier(const char *text)
{
	if (!*text || isdigit((unsigned char)*text)) return 0;
	while (*text) if (!word(*text++)) return 0;
	return 1;
}

/* This namespace cannot be named by source, including forward references. */
int
macro_reserved(const char *text)
{
	char normalized[255];
	int quote = 0;
	if (strlen(text) >= sizeof(normalized)) return 1;
	strcpy(normalized, text);
	macro_join(normalized);
	text = normalized;
	while (*text) {
		if (quote) {
			if (*text == quote) quote = 0;
			++text;
		} else if (*text == ';') break;
		else if (*text == '\'' || *text == '"') quote = *text++;
		else if (word(*text)) {
			char token[256];
			size_t n = 0;
			while (word(*text)) token[n++] = (char)tolower((unsigned char)*text++);
			token[n] = 0;
			if (!strncmp(token, "__m80_", 6)) return 1;
		} else ++text;
	}
	return 0;
}

macro_t *
macro_find(macro_t *macros, const char *name)
{
	for (; macros; macros = macros->next)
		if (symbol_equal(macros->name, name)) return macros;
	return NULL;
}

/* Commas inside strings or angle groups belong to the argument. One outer
 * angle pair is removed. Missing actual parameters are empty text. */
const char *
macro_actuals(const char *text, char values[32][256], size_t *count,
    macro_eval_t evaluate, void *context)
{
	const char *p = text;
	*count = 0;
	if (!*p) return NULL;
	for (;;) {
		char value[256];
		size_t used = 0, protected = 0;
		int angle = 0, quote = 0, grouped, numeric, closed = 0;
		while (*p == ' ' || *p == '\t') ++p;
		grouped = *p == '<'; numeric = *p == '%' && evaluate;
		if (grouped) { ++p; angle = 1; }
		else if (numeric) ++p;
		for (; *p; ++p) {
			char ch = *p;
			if (closed) {
				if (ch == ',') break;
				if (ch == ' ' || ch == '\t') continue;
				return "text after macro argument angle group";
			}
			if (!quote && ch == '!') {
				if (!p[1]) return "dangling ! in macro argument";
				ch = *++p;
				protected = used + 1;
			} else if (quote) { if (ch == quote) quote = 0; }
			else if (ch == '\'' || ch == '"') quote = ch;
			else if (ch == '<' && !numeric) ++angle;
			else if (ch == '>' && !numeric) {
				if (!angle) return "unmatched macro argument bracket";
				if (!--angle && grouped) { closed = 1; continue; }
			} else if (ch == ',' && !angle) break;
			if (used == sizeof(value) - 1) return "macro argument limit exceeded";
			value[used++] = ch;
		}
		if (quote || angle) return "unclosed macro argument string or bracket";
		if (!grouped) while (used > protected && (value[used - 1] == ' ' || value[used - 1] == '\t')) --used;
		value[used] = 0;
		if (*count == 32) return "macro argument limit exceeded";
		if (numeric) {
			long number;
			if (!evaluate(context, value, &number)) return "% requires a previously defined absolute value";
			/* The reader currently has the default decimal radix only. */
			snprintf(value, sizeof(value), "%lu", (unsigned long)number & 65535UL);
		}
		strcpy(values[(*count)++], value);
		if (!*p) return NULL;
		++p;
	}
}

const char *
macro_arguments(const char *text, char values[32][256], size_t *count)
{
	return macro_actuals(text, values, count, NULL, NULL);
}

/* Strip one required angle group, preserving its interior exactly. */
static const char *
condition_arg(const char **cursor, char out[256])
{
	const char *p = *cursor;
	size_t used = 0;
	int depth = 1, quote = 0;
	while (*p == ' ' || *p == '\t') ++p;
	if (*p++ != '<') return "string condition requires angle brackets";
	for (; *p; ++p) {
		char ch = *p;
		if (!quote && ch == '!') {
			if (!p[1]) return "dangling ! in string condition";
			ch = *++p;
		} else if (quote) { if (ch == quote) quote = 0; }
		else if (ch == '\'' || ch == '"') quote = ch;
		else if (ch == '<') ++depth;
		else if (ch == '>' && !--depth) {
			out[used] = 0; *cursor = p + 1; return NULL;
		}
		if (used == 255) return "string condition argument too long";
		out[used++] = ch;
	}
	return "unclosed string condition angle group";
}

const char *
macro_condition(const statement_t *stmt, int *test)
{
	char left[256], right[256];
	const char *cursor = stmt->first, *error;
	int compare = !strcmp(stmt->opcode, "ifidn") || !strcmp(stmt->opcode, "ifdif");
	if ((error = condition_arg(&cursor, left))) return error;
	while (*cursor == ' ' || *cursor == '\t') ++cursor;
	if (compare) {
		if (*cursor++ != ',') return "string comparison requires two angle groups";
		if ((error = condition_arg(&cursor, right))) return error;
		while (*cursor == ' ' || *cursor == '\t') ++cursor;
		*test = !strcmp(left, right);
		if (!strcmp(stmt->opcode, "ifdif")) *test = !*test;
	} else {
		*test = !left[0];
		if (!strcmp(stmt->opcode, "ifnb")) *test = !*test;
	}
	if (*cursor || stmt->second[0]) return "unexpected text after string condition";
	return NULL;
}

/* Ampersands separate macro tokens through nested expansions; the final
 * statement joins them before binding names in the shared expression IR. */
void
macro_join(char *text)
{
	char *out = text;
	int quote = 0;
	while (*text) {
		if (*text == '\'' || *text == '"') {
			if (!quote) quote = *text;
			else if (*text == quote) quote = 0;
		}
		if (!quote && (word(*text) || (*text == '&' && word(text[1])))) {
			char *end = text;
			int symbol = !isdigit((unsigned char)*text);
			while (word(*end) || *end == '&') ++end;
			while (text < end) {
				if (*text != '&' || !symbol) *out++ = *text;
				++text;
			}
		} else *out++ = *text++;
	}
	*out = 0;
}

static const char *
operands(const statement_t *stmt, char values[32][256], size_t *count)
{
	char text[514];
	snprintf(text, sizeof(text), "%s%s%s", stmt->first, stmt->second[0] ? "," : "", stmt->second);
	return macro_arguments(text, values, count);
}

void
macro_destroy(macro_t *macros)
{
	while (macros) {
		macro_t *next = macros->next;
		source_destroy(&macros->body);
		free(macros);
		macros = next;
	}
}

const char *
macro_define(macro_t **macros, const source_program_t *raw, size_t first, size_t last, const char *path)
{
	const statement_t *head = &raw->records[first].statement;
	macro_t *macro = calloc(1, sizeof(*macro));
	const char *error = NULL;
	size_t i, j, k;
	int body_started = 0;
	if (!macro) return "out of memory defining macro";
	if (!identifier(head->label)) { error = "MACRO requires a name"; goto fail; }
	if (macro_find(*macros, head->label)) { error = "duplicate macro definition"; goto fail; }
	{
		statement_t probe;
		m80_statement(head->label, &probe, 0);
		if (probe.opcode[0]) { error = "macro name conflicts with an operation"; goto fail; }
	}
	strcpy(macro->name, head->label);
	if ((error = operands(head, macro->parameters, &macro->parameter_count))) goto fail;
	for (i = 0; i < macro->parameter_count; ++i) {
		if (!identifier(macro->parameters[i])) { error = "invalid macro parameter name"; goto fail; }
		for (j = 0; j < i; ++j) if (symbol_equal(macro->parameters[i], macro->parameters[j])) {
			error = "duplicate macro parameter"; goto fail;
		}
	}
	macro->body.records = calloc(last - first, sizeof(*macro->body.records));
	if (!macro->body.records) { error = "out of memory defining macro"; goto fail; }
	for (i = first + 1; i < last; ++i) {
		const source_record_t *record = &raw->records[i];
		const statement_t *stmt = &record->statement;
		if (!strcmp(stmt->opcode, "macro")) { error = "nested macro definitions are not implemented"; goto fail; }
		if (!strcmp(stmt->opcode, "local")) {
			char names[32][256]; size_t count;
			if (body_started || stmt->label[0] || record->problem) { error = "LOCAL must precede the macro body"; goto fail; }
			if ((error = operands(stmt, names, &count))) goto fail;
			if (!count || count + macro->local_count > 32) { error = "invalid LOCAL list or local limit exceeded"; goto fail; }
			for (j = 0; j < count; ++j) {
				if (!identifier(names[j])) { error = "invalid LOCAL name"; goto fail; }
				for (k = 0; k < macro->parameter_count; ++k)
					if (symbol_equal(names[j], macro->parameters[k])) { error = "LOCAL conflicts with parameter"; goto fail; }
				for (k = 0; k < macro->local_count; ++k)
					if (symbol_equal(names[j], macro->locals[k])) { error = "duplicate LOCAL name"; goto fail; }
				strcpy(macro->locals[macro->local_count++], names[j]);
			}
			continue;
		}
		if (stmt->opcode[0] || stmt->label[0]) body_started = 1;
		macro->body.records[macro->body.count] = *record;
		macro->body.records[macro->body.count].origin_file = path ? strdup(path) : NULL;
		if (path && !macro->body.records[macro->body.count].origin_file) { error = "out of memory defining macro"; goto fail; }
		++macro->body.count;
	}
	macro->next = *macros; *macros = macro;
	return NULL;
fail:
	macro_destroy(macro);
	return error;
}

static const char *
substitute(const char *text, const macro_t *macro, char actual[32][256], char locals[32][256], char out[255])
{
	size_t used = 0;
	int quote = 0, comment = 0;
	while (*text) {
		const char *replacement = text;
		char token[256];
		size_t length = 1, i;
		if (!comment && !quote && *text == ';') {
			if (text[1] == ';') break;
			comment = 1;
		}
		if (!comment && *text == '&' && quote && word(text[1])) {
			const char *next = text + 1;
			size_t n = 0;
			while (word(next[n])) { token[n] = next[n]; ++n; }
			token[n] = 0;
			for (i = 0; i < macro->parameter_count; ++i)
				if (symbol_equal(token, macro->parameters[i])) replacement = actual[i];
			for (i = 0; i < macro->local_count; ++i)
				if (symbol_equal(token, macro->locals[i])) replacement = locals[i];
			if (replacement != text) length = n + 1;
		}
		if (!comment && (*text == '\'' || *text == '"')) {
			if (!quote) quote = *text;
			else if (quote == *text) quote = 0;
		} else if (!quote && !comment && word(*text)) {
			length = 0;
			while (word(text[length])) { token[length] = text[length]; ++length; }
			token[length] = 0;
			for (i = 0; i < macro->parameter_count; ++i)
				if (symbol_equal(token, macro->parameters[i])) replacement = actual[i];
			for (i = 0; i < macro->local_count; ++i)
				if (symbol_equal(token, macro->locals[i])) replacement = locals[i];
		}
		i = replacement == text ? length : strlen(replacement);
		if (used + i > 254) return "expanded macro line exceeds 254 characters";
		memcpy(out + used, replacement, i); used += i;
		text += length;
	}
	out[used] = 0;
	return NULL;
}

static const char *
expand_values(const macro_t *macro, char actual[32][256], unsigned long *serial, source_program_t *out)
{
	char locals[32][256];
	size_t i;
	for (i = 0; i < macro->local_count; ++i)
		snprintf(locals[i], sizeof(locals[i]), "__m80_%lu", ++*serial);
	out->records = calloc(macro->body.count ? macro->body.count : 1, sizeof(*out->records));
	if (!out->records) return "out of memory expanding macro";
	for (i = 0; i < macro->body.count; ++i) {
		const source_record_t *body = &macro->body.records[i];
		source_record_t *record = &out->records[out->count++];
		record->origin_line = body->origin_line;
		record->origin_file = body->origin_file ? strdup(body->origin_file) : NULL;
		if (body->origin_file && !record->origin_file) return "out of memory expanding macro";
		if (!strcmp(body->statement.opcode, "irp") || !strcmp(body->statement.opcode, "irpc")) {
			size_t j;
			for (j = 0; j < macro->parameter_count; ++j)
				if (symbol_equal(body->statement.first, macro->parameters[j]))
					record->problem = "nested repetition parameter shadowing is not implemented";
			for (j = 0; j < macro->local_count; ++j)
				if (symbol_equal(body->statement.first, macro->locals[j]))
					record->problem = "nested repetition parameter conflicts with LOCAL";
		}
		if (!record->problem) record->problem = substitute(body->text, macro, actual, locals, record->text);
		if (record->problem) {
			/* Diagnose only if this record executes: EXITM and inactive blocks
			 * may discard it. Keep structure available to block matching. */
			strcpy(record->text, body->text);
			record->statement = body->statement;
		} else {
			const char *problem = m80_statement(record->text, &record->statement, 0);
			record->problem = body->problem ? body->problem : problem;
		}
	}
	return NULL;
}

/* Capture a repetition body with the same ownership contract as a macro. */
const char *
macro_block(macro_t **block, const source_program_t *raw, size_t first, size_t last, const char *path)
{
	source_program_t view = {0};
	const char *error;
	size_t i;
	view.count = last - first + 1;
	view.records = malloc(view.count * sizeof(*view.records));
	if (!view.records) return "out of memory capturing repetition";
	memcpy(view.records, raw->records + first, view.count * sizeof(*view.records));
	for (i = 1; i + 1 < view.count; ++i)
		if (!strcmp(view.records[i].statement.opcode, "local")) {
			free(view.records); return "LOCAL is only allowed at the start of a MACRO definition";
		}
	if (!strcmp(view.records[0].statement.opcode, "rept")) view.records[0].statement.first[0] = 0;
	strcpy(view.records[0].statement.label, "_block");
	strcpy(view.records[0].statement.opcode, "macro");
	view.records[0].statement.second[0] = 0;
	error = macro_define(block, &view, 0, view.count - 1, path);
	free(view.records); /* Borrowed record paths remain owned by raw. */
	return error;
}

const char *
macro_expand(const macro_t *macro, const statement_t *call, macro_eval_t evaluate, void *context, unsigned long *serial, source_program_t *out)
{
	char actual[32][256] = {{0}};
	size_t count;
	char text[514];
	const char *error;
	snprintf(text, sizeof(text), "%s%s%s", call->first, call->second[0] ? "," : "", call->second);
	error = macro_actuals(text, actual, &count, evaluate, context);
	if (error) return error;
	if (count > macro->parameter_count) return "too many macro arguments";
	return expand_values(macro, actual, serial, out);
}

const char *
macro_iteration(const macro_t *macro, const char *value, unsigned long *serial, source_program_t *out)
{
	char actual[32][256] = {{0}};
	if (strlen(value) >= sizeof(actual[0])) return "iteration argument too long";
	strcpy(actual[0], value);
	return expand_values(macro, actual, serial, out);
}
