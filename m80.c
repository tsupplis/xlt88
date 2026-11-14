#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "symbols.h"
#include "macro.h"

typedef struct m80_symbol {
	char name[256];
	long value;
	int known, mutable;
} m80_symbol_t;
typedef struct conditional {
	int parent, test, otherwise;
} conditional_t;
typedef struct m80_context {
	source_program_t *output;
	macro_t *macros;
	unsigned macro_depth, exit_depth;
	size_t expansions;
	unsigned long serial;
	m80_symbol_t *symbols;
	size_t count, capacity, read_count;
	struct stat stack[32];
	int ended, failed, oom;
} m80_context_t;

static int
append(m80_context_t *ctx, const source_record_t *record, const char *path, const char *problem, int directive)
{
	source_program_t *out = ctx->output;
	source_record_t copy = *record;
	if (out->count >= 65536) { ctx->oom = 1; return 0; }
	if (out->count == ctx->capacity) {
		size_t capacity = ctx->capacity ? ctx->capacity * 2 : 64;
		void *items = realloc(out->records, capacity * sizeof(*out->records));
		if (!items) { ctx->oom = 1; return 0; }
		out->records = items; ctx->capacity = capacity;
	}
	copy.origin_file = path ? strdup(path) : NULL;
	if (path && !copy.origin_file) { ctx->oom = 1; return 0; }
	copy.line = out->count + 1;
	copy.problem = problem;
	if (directive) {
		memset(&copy.statement, 0, sizeof(copy.statement));
		strcpy(copy.statement.comment, "; M80 preprocessor directive");
	}
	out->records[out->count++] = copy;
	if (problem) ctx->failed = 1;
	return !problem;
}

static m80_symbol_t *
find(m80_context_t *ctx, const char *name)
{
	size_t i;
	for (i = 0; i < ctx->count; ++i)
		if (symbol_equal(ctx->symbols[i].name, name)) return &ctx->symbols[i];
	return NULL;
}

static int
lookup(void *context, size_t id, const char *name, long *value)
{
	m80_symbol_t *item = find(context, name);
	(void)id;
	if (!item || !item->known) return 0;
	*value = item->value;
	return 1;
}

static int
evaluate(void *context, const char *text, long *value)
{
	const char *error;
	char normalized[256];
	expr_t *tree;
	if (strlen(text) >= sizeof(normalized)) return 0;
	strcpy(normalized, text);
	macro_join(normalized);
	tree = expr_parse(normalized, NULL, NULL, 0, &error);
	int result = expr_eval(tree, lookup, context, -1, value);
	expr_destroy(tree);
	return result;
}

static int
define(m80_context_t *ctx, const statement_t *stmt)
{
	m80_symbol_t *item;
	void *items;
	if (!stmt->label[0]) return 1;
	if (find(ctx, stmt->label)) return 1; /* Shared symbols diagnose duplicates. */
	items = realloc(ctx->symbols, (ctx->count + 1) * sizeof(*ctx->symbols));
	if (!items) { ctx->oom = 1; return 0; }
	ctx->symbols = items;
	item = &ctx->symbols[ctx->count++];
	memset(item, 0, sizeof(*item));
	strcpy(item->name, stmt->label);
	if (!strcmp(stmt->opcode, "equ")) item->known = evaluate(ctx, stmt->first, &item->value);
	return 1;
}

static const char *
set_value(m80_context_t *ctx, const statement_t *stmt)
{
	const char *error;
	expr_t *name = expr_parse(stmt->label, NULL, NULL, 0, &error);
	m80_symbol_t *item = find(ctx, stmt->label);
	long value;
	int valid = name && name->kind == expr_symbol;
	expr_destroy(name);
	if (!valid || stmt->second[0]) return "SET requires a symbol and one absolute expression";
	if (item && !item->mutable) return "SET cannot redefine an EQU or address label";
	if (!evaluate(ctx, stmt->first, &value)) return "SET requires a previously defined absolute value";
	if (!item) {
		if (!define(ctx, stmt)) return "out of memory defining SET symbol";
		item = find(ctx, stmt->label);
	}
	item->known = item->mutable = 1;
	item->value = (long)((unsigned long)value & 65535UL);
	return NULL;
}

static int
freeze_text(m80_context_t *ctx, char text[256])
{
	char result[256];
	const char *p = text;
	size_t used = 0;
	int quote = 0;
	while (*p) {
		char token[256], number[32];
		const char *replacement = p;
		size_t length = 1, output;
		if (*p == '\'' || *p == '"') {
			if (!quote) quote = *p;
			else if (quote == *p) quote = 0;
		} else if (!quote && (isalnum((unsigned char)*p) || strchr("_.$?@#", *p))) {
			m80_symbol_t *item;
			length = 0;
			while (p[length] && (isalnum((unsigned char)p[length]) || strchr("_.$?@#", p[length]))) {
				token[length] = p[length]; ++length;
			}
			token[length] = 0;
			item = find(ctx, token);
			if (item && item->mutable) {
				snprintf(number, sizeof(number), "%ld", item->value);
				replacement = number;
			}
		}
		output = replacement == p ? length : strlen(replacement);
		if (used + output >= sizeof(result)) return 0;
		memcpy(result + used, replacement, output); used += output; p += length;
	}
	result[used] = 0;
	strcpy(text, result);
	return 1;
}

static int
freeze_statement(m80_context_t *ctx, statement_t *stmt)
{
	static const char *const registers[] = {
		"mov", "add", "adc", "sub", "sbb", "ana", "xra", "ora", "cmp",
		"inr", "dcr", "inx", "dcx", "dad", "ldax", "stax", "push", "pop"
	};
	size_t i;
	if (!strcmp(stmt->opcode, "mvi") || !strcmp(stmt->opcode, "lxi"))
		return freeze_text(ctx, stmt->second);
	for (i = 0; i < sizeof(registers)/sizeof(registers[0]); ++i)
		if (!strcmp(stmt->opcode, registers[i])) return 1;
	return freeze_text(ctx, stmt->first) && freeze_text(ctx, stmt->second);
}

static int
remember(m80_context_t *ctx, const char *path)
{
	source_program_t *out = ctx->output;
	char **files;
	if (!path) return 1;
	if (out->file_count >= 1024) return 0;
	files = realloc(out->files, (out->file_count + 1) * sizeof(*files));
	if (!files) return 0;
	out->files = files;
	out->files[out->file_count] = strdup(path);
	if (!out->files[out->file_count]) return 0;
	++out->file_count;
	return 1;
}

static char *
include_path(const char *parent, const char *operand)
{
	const char *slash = parent ? strrchr(parent, '/') : NULL;
	size_t directory = slash ? (size_t)(slash - parent + 1) : 0;
	size_t length = strlen(operand);
	char *path;
	if (!length) return NULL;
	if (*operand == '\'' || *operand == '"') {
		if (length < 3 || operand[length - 1] != *operand) return NULL;
		++operand; length -= 2;
	} else if (strpbrk(operand, " \t<>")) return NULL;
	if (*operand == '/') directory = 0;
	path = malloc(directory + length + 1);
	if (!path) return NULL;
	if (directory) memcpy(path, parent, directory);
	memcpy(path + directory, operand, length);
	path[directory + length] = 0;
	return path;
}

static int process(m80_context_t *, FILE *, const char *, unsigned);
static int process_records(m80_context_t *, const source_program_t *, const char *, unsigned);

static int
expand_run(m80_context_t *ctx, const macro_t *macro, const source_record_t *record,
    const char *value, const char *path, unsigned depth)
{
	source_program_t expanded = {0};
	const char *error = record->problem;
	int exited = 0;
	if (ctx->macro_depth >= 32) error = "macro nesting exceeds 32";
	else if (ctx->expansions++ >= 65536) error = "macro/repetition expansion budget exceeded";
	if (!error) error = value ? macro_iteration(macro, value, &ctx->serial, &expanded) :
	    macro_expand(macro, &record->statement, evaluate, ctx, &ctx->serial, &expanded);
	if (error) append(ctx, record, path, error, 0);
	else {
		++ctx->macro_depth;
		process_records(ctx, &expanded, path, depth);
		if (ctx->exit_depth == ctx->macro_depth) { ctx->exit_depth = 0; exited = 1; }
		--ctx->macro_depth;
	}
	source_destroy(&expanded);
	return exited;
}

static void
repeat_run(m80_context_t *ctx, const source_program_t *raw, size_t first, size_t last,
    const source_record_t *record, const char *path, unsigned depth)
{
	const statement_t *stmt = &record->statement;
	macro_t *block = NULL;
	char values[32][256] = {{0}}, text[256], character[2] = {0};
	size_t count = 0, i, length = strlen(stmt->second);
	int rept = !strcmp(stmt->opcode, "rept"), irpc = !strcmp(stmt->opcode, "irpc");
	const char *error = record->problem;
	long repetitions;
	if (!error && (stmt->label[0] || !stmt->first[0] || (!rept && !strchr(record->text, ',')))) error = "invalid repetition directive";
	if (!error && rept) {
		if (stmt->second[0] || !evaluate(ctx, stmt->first, &repetitions) || repetitions < 0 || repetitions > 65535)
			error = "REPT requires a known absolute count from 0 to 65535";
		else count = (size_t)repetitions;
	} else if (!error && irpc) {
		size_t arguments;
		error = macro_arguments(stmt->second, values, &arguments);
		if (!error && arguments > 1) error = "IRPC requires a single text argument";
		if (!error) {
			strcpy(text, values[0]);
			count = strlen(text);
			if (!count) count = 1;
		}
	} else if (!error) {
		if (length >= 2 && stmt->second[0] == '<' && stmt->second[length - 1] == '>') {
			memcpy(text, stmt->second + 1, length - 2); text[length - 2] = 0;
			error = macro_actuals(text, values, &count, evaluate, ctx);
			if (!count) count = 1; /* Empty IRP expands once with empty text. */
		} else error = "IRP requires an angle-bracketed list";
	}
	if (!error) error = macro_block(&block, raw, first, last, path);
	if (error) append(ctx, record, path, error, 0);
	else for (i = 0; i < count && !ctx->failed && !ctx->oom && !ctx->ended; ++i) {
		const char *value = "";
		if (irpc) { character[0] = text[i]; value = character; }
		else if (!rept) value = values[i];
		if (expand_run(ctx, block, record, value, path, depth)) break;
	}
	macro_destroy(block);
}

static int
process_records(m80_context_t *ctx, const source_program_t *raw, const char *path, unsigned depth)
{
	conditional_t conditions[64];
	size_t level = 0, i;
	int active = 1;
	source_record_t location = { .line = 1, .origin_line = 1 };
	for (i = 0; i < raw->count && !ctx->ended && !ctx->failed && !ctx->oom && !ctx->exit_depth; ++i) {
		const source_record_t *record = &raw->records[i];
		source_record_t parsed = *record;
		const char *origin = record->origin_file ? record->origin_file : path;
		const statement_t *stmt;
		macro_join(parsed.statement.label);
		macro_join(parsed.statement.opcode);
		/* A known macro in column one is an operation, not a label. */
		if (macro_find(ctx->macros, parsed.statement.label)) {
			const char *token = parsed.text;
			while (*token && *token != ':' && *token != ' ' && *token != '\t') ++token;
			if (*token != ':') parsed.problem = m80_statement(parsed.text, &parsed.statement, 1);
		}
		macro_join(parsed.statement.opcode);
		record = &parsed;
		stmt = &record->statement;
		const char *op = stmt->opcode;
		int opening = !strcmp(op, "if") || !strcmp(op, "ift") || !strcmp(op, "ife") ||
		    !strcmp(op, "iff") || !strcmp(op, "ifdef") || !strcmp(op, "ifndef") ||
		    !strcmp(op, "if1") || !strcmp(op, "if2") || !strcmp(op, "ifb") ||
		    !strcmp(op, "ifnb") || !strcmp(op, "ifidn") || !strcmp(op, "ifdif");
		location = *record;
		if (opening) {
			long value = 0;
			int test = 0;
			if (level == 64) { append(ctx, record, origin, "conditional nesting exceeds 64", 0); break; }
			if (active) {
				if (record->problem || stmt->label[0] || stmt->second[0] || !stmt->first[0]) {
					append(ctx, record, origin, record->problem ? record->problem : "invalid conditional directive", 0); break;
				}
				if (!strcmp(op, "ifb") || !strcmp(op, "ifnb") || !strcmp(op, "ifidn") || !strcmp(op, "ifdif")) {
					const char *error = macro_condition(stmt, &test);
					if (error) { append(ctx, record, origin, error, 0); break; }
				} else if (!strcmp(op, "ifdef") || !strcmp(op, "ifndef")) {
					const char *error;
					macro_join(parsed.statement.first);
					expr_t *name = expr_parse(stmt->first, NULL, NULL, 0, &error);
					int valid = name && name->kind == expr_symbol;
					expr_destroy(name);
					if (!valid) {
						append(ctx, record, origin, "IFDEF/IFNDEF requires a symbol name", 0); break;
					}
					test = find(ctx, stmt->first) != NULL;
					if (!strcmp(op, "ifndef")) test = !test;
				} else {
					if (!evaluate(ctx, stmt->first, &value)) {
						append(ctx, record, origin, "condition requires a previously defined absolute value", 0); break;
					}
					test = value != 0;
					if (!strcmp(op, "ife") || !strcmp(op, "iff")) test = !test;
				}
			}
			conditions[level++] = (conditional_t){active, test, 0};
			active = active && test;
			append(ctx, record, origin, NULL, 1);
			continue;
		}
		if (!strcmp(op, "else") || !strcmp(op, "endif")) {
			if (!level || stmt->label[0] || stmt->first[0] || stmt->second[0] || record->problem) {
				append(ctx, record, origin, "unmatched or invalid ELSE/ENDIF", 0); break;
			}
			if (!strcmp(op, "else")) {
				conditional_t *condition = &conditions[level - 1];
				if (condition->otherwise) { append(ctx, record, origin, "duplicate ELSE", 0); break; }
				condition->otherwise = 1;
				active = condition->parent && !condition->test;
			} else active = conditions[--level].parent;
			append(ctx, record, origin, NULL, 1);
			continue;
		}
		if (!strcmp(op, "macro") || !strcmp(op, "rept") || !strcmp(op, "irp") || !strcmp(op, "irpc")) {
			size_t last = i + 1, nesting = 1;
			const char *error = active ? record->problem : NULL;
			for (; last < raw->count; ++last) {
				const statement_t *body = &raw->records[last].statement;
				if (!strcmp(body->opcode, "macro") || !strcmp(body->opcode, "rept") ||
				    !strcmp(body->opcode, "irp") || !strcmp(body->opcode, "irpc")) {
					++nesting;
					if (active && !strcmp(body->opcode, "macro")) error = "nested macro definitions are not implemented";
				}
				if (!strcmp(body->opcode, "endm") && !--nesting) break;
			}
			if (!error && last == raw->count) error = "missing ENDM for macro";
			if (!error) {
				const source_record_t *end = &raw->records[last];
				if (end->problem || end->statement.label[0] || end->statement.first[0] || end->statement.second[0])
					error = "invalid ENDM";
			}
			if (!error && active && !strcmp(op, "macro")) error = macro_define(&ctx->macros, raw, i, last, origin);
			if (!error && active && strcmp(op, "macro")) repeat_run(ctx, raw, i, last, record, origin, depth);
			append(ctx, record, origin, error, !error);
			i = last;
			continue;
		}
		if (!active) continue;
		if (!strcmp(op, "set")) {
			const char *error = record->problem ? record->problem : set_value(ctx, stmt);
			append(ctx, record, origin, error, !error);
			continue;
		}
		if (stmt->label[0]) {
			m80_symbol_t *item = find(ctx, stmt->label);
			if (item && item->mutable) {
				append(ctx, record, origin, "SET symbol cannot become an EQU or address label", 0); break;
			}
		}
		if (!strcmp(op, "exitm")) {
			if (!ctx->macro_depth || stmt->label[0] || stmt->first[0] || stmt->second[0] || record->problem)
				append(ctx, record, origin, "EXITM requires an expansion and no operands", 0);
			else { append(ctx, record, origin, NULL, 1); ctx->exit_depth = ctx->macro_depth; }
			break;
		}
		if (!strcmp(op, "endm") || !strcmp(op, "local")) {
			append(ctx, record, origin, "ENDM/LOCAL outside a macro definition", 0); break;
		}
		{
			macro_t *macro = macro_find(ctx->macros, op);
			if (macro) {
				if (stmt->label[0]) {
					source_record_t label = *record;
					memset(&label.statement, 0, sizeof(label.statement));
					strcpy(label.statement.label, stmt->label);
					define(ctx, &label.statement);
					append(ctx, &label, origin, NULL, 0);
				} else append(ctx, record, origin, NULL, 1);
				expand_run(ctx, macro, record, NULL, origin, depth);
				continue;
			}
		}
		if (record->problem) { append(ctx, record, origin, record->problem, 0); break; }
		if (!strcmp(op, "include") || !strcmp(op, ".include")) {
			char *included = NULL;
			FILE *stream = NULL;
			struct stat identity;
			const char *error = NULL;
			unsigned j;
			if (stmt->label[0] || stmt->second[0]) error = "invalid INCLUDE directive";
			else if (depth + 1 >= 32) error = "include nesting exceeds 32";
			else if (!path) error = "INCLUDE needs a source path; use source_read";
			else if (!(included = include_path(origin, stmt->first))) error = "invalid INCLUDE filename";
			else if (!(stream = fopen(included, "r"))) error = "cannot open included source";
			else if (fstat(fileno(stream), &identity) < 0) error = "cannot inspect included source";
			else for (j = 0; j <= depth; ++j)
				if (identity.st_dev == ctx->stack[j].st_dev && identity.st_ino == ctx->stack[j].st_ino)
					error = "include cycle detected";
			if (error) append(ctx, record, origin, error, 0);
			else {
				append(ctx, record, origin, NULL, 1);
				process(ctx, stream, included, depth + 1);
			}
			if (stream) fclose(stream);
			free(included);
			continue;
		}
		if (!strcmp(op, "end")) {
			if (level) { append(ctx, record, origin, "END before matching ENDIF", 0); break; }
			ctx->ended = 1;
		}
		macro_join(parsed.statement.label);
		macro_join(parsed.statement.first);
		macro_join(parsed.statement.second);
		if (!freeze_statement(ctx, &parsed.statement)) {
			append(ctx, record, origin, "SET substitution exceeds operand size limit", 0); break;
		}
		if (!define(ctx, stmt)) break;
		append(ctx, record, origin, NULL, 0);
	}
	if (level && !ctx->failed && !ctx->oom && !ctx->exit_depth) append(ctx, &location, path, "missing ENDIF in source file", 0);
	return !ctx->failed && !ctx->oom;
}

static int
process(m80_context_t *ctx, FILE *input, const char *path, unsigned depth)
{
	source_program_t raw = {0};
	source_record_t location = { .line = 1, .origin_line = 1 };
	size_t i;
	if (!remember(ctx, path)) { ctx->oom = 1; return 0; }
	if (fstat(fileno(input), &ctx->stack[depth]) < 0 || !m80_raw_read(input, &raw)) {
		append(ctx, &location, path, "cannot read M80 source", 0);
		source_destroy(&raw); return 0;
	}
	if (raw.count > 65536 - ctx->read_count) {
		append(ctx, &location, path, "M80 source expansion exceeds 65536 records", 0);
		source_destroy(&raw); return 0;
	}
	ctx->read_count += raw.count;
	for (i = 0; i < raw.count; ++i) {
		if (macro_reserved(raw.records[i].text)) {
			append(ctx, &raw.records[i], path, "__m80_ is reserved for generated macro locals", 0);
			break;
		}
	}
	if (!ctx->failed) process_records(ctx, &raw, path, depth);
	source_destroy(&raw);
	return !ctx->failed && !ctx->oom;
}

int
m80_read_file(FILE *input, const char *path, source_program_t *program)
{
	m80_context_t ctx = { .output = program };
	process(&ctx, input, path, 0);
	free(ctx.symbols);
	macro_destroy(ctx.macros);
	return !ctx.oom; /* Syntax failures are retained as diagnostic records. */
}
