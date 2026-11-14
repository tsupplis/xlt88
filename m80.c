#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "symbols.h"

typedef struct m80_symbol {
	char name[256];
	long value;
	int known;
} m80_symbol_t;
typedef struct conditional {
	int parent, test, otherwise;
} conditional_t;
typedef struct m80_context {
	source_program_t *output;
	m80_symbol_t *symbols;
	size_t count, capacity;
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
evaluate(m80_context_t *ctx, const char *text, long *value)
{
	const char *error;
	expr_t *tree = expr_parse(text, NULL, NULL, 0, &error);
	int result = expr_eval(tree, lookup, ctx, -1, value);
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

static int
process(m80_context_t *ctx, FILE *input, const char *path, unsigned depth)
{
	source_program_t raw = {0};
	conditional_t conditions[64];
	size_t level = 0, i;
	int active = 1;
	source_record_t location = { .line = 1, .origin_line = 1 };
	if (!remember(ctx, path)) { ctx->oom = 1; return 0; }
	if (fstat(fileno(input), &ctx->stack[depth]) < 0 || !m80_raw_read(input, &raw)) {
		append(ctx, &location, path, "cannot read M80 source", 0);
		source_destroy(&raw); return 0;
	}
	for (i = 0; i < raw.count && !ctx->ended && !ctx->failed && !ctx->oom; ++i) {
		const source_record_t *record = &raw.records[i];
		const statement_t *stmt = &record->statement;
		const char *op = stmt->opcode;
		int opening = !strcmp(op, "if") || !strcmp(op, "ift") || !strcmp(op, "ife") ||
		    !strcmp(op, "iff") || !strcmp(op, "ifdef") || !strcmp(op, "ifndef");
		location = *record;
		if (opening) {
			long value = 0;
			int test = 0;
			if (level == 64) { append(ctx, record, path, "conditional nesting exceeds 64", 0); break; }
			if (active) {
				if (record->problem || stmt->label[0] || stmt->second[0] || !stmt->first[0]) {
					append(ctx, record, path, "invalid conditional directive", 0); break;
				}
				if (!strcmp(op, "ifdef") || !strcmp(op, "ifndef")) {
					/* A symbol name, not an arithmetic expression. */
					if (strpbrk(stmt->first, " \t()+-*/,")) {
						append(ctx, record, path, "IFDEF/IFNDEF requires a symbol name", 0); break;
					}
					test = find(ctx, stmt->first) != NULL;
					if (!strcmp(op, "ifndef")) test = !test;
				} else {
					if (!evaluate(ctx, stmt->first, &value)) {
						append(ctx, record, path, "condition requires a previously defined absolute value", 0); break;
					}
					test = value != 0;
					if (!strcmp(op, "ife") || !strcmp(op, "iff")) test = !test;
				}
			}
			conditions[level++] = (conditional_t){active, test, 0};
			active = active && test;
			append(ctx, record, path, NULL, 1);
			continue;
		}
		if (!strcmp(op, "else") || !strcmp(op, "endif")) {
			if (!level || stmt->label[0] || stmt->first[0] || stmt->second[0] || record->problem) {
				append(ctx, record, path, "unmatched or invalid ELSE/ENDIF", 0); break;
			}
			if (!strcmp(op, "else")) {
				conditional_t *condition = &conditions[level - 1];
				if (condition->otherwise) { append(ctx, record, path, "duplicate ELSE", 0); break; }
				condition->otherwise = 1;
				active = condition->parent && !condition->test;
			} else active = conditions[--level].parent;
			append(ctx, record, path, NULL, 1);
			continue;
		}
		if (!active) continue;
		if (record->problem) { append(ctx, record, path, record->problem, 0); break; }
		if (!strcmp(op, "include") || !strcmp(op, ".include")) {
			char *included = NULL;
			FILE *stream = NULL;
			struct stat identity;
			const char *error = NULL;
			unsigned j;
			if (stmt->label[0] || stmt->second[0]) error = "invalid INCLUDE directive";
			else if (depth + 1 >= 32) error = "include nesting exceeds 32";
			else if (!path) error = "INCLUDE needs a source path; use source_read";
			else if (!(included = include_path(path, stmt->first))) error = "invalid INCLUDE filename";
			else if (!(stream = fopen(included, "r"))) error = "cannot open included source";
			else if (fstat(fileno(stream), &identity) < 0) error = "cannot inspect included source";
			else for (j = 0; j <= depth; ++j)
				if (identity.st_dev == ctx->stack[j].st_dev && identity.st_ino == ctx->stack[j].st_ino)
					error = "include cycle detected";
			if (error) append(ctx, record, path, error, 0);
			else {
				append(ctx, record, path, NULL, 1);
				process(ctx, stream, included, depth + 1);
			}
			if (stream) fclose(stream);
			free(included);
			continue;
		}
		if (!strcmp(op, "end")) {
			if (level) { append(ctx, record, path, "END before matching ENDIF", 0); break; }
			ctx->ended = 1;
		}
		if (!define(ctx, stmt)) break;
		append(ctx, record, path, NULL, 0);
	}
	if (level && !ctx->failed && !ctx->oom) append(ctx, &location, path, "missing ENDIF in source file", 0);
	source_destroy(&raw);
	return !ctx->failed && !ctx->oom;
}

int
m80_read_file(FILE *input, const char *path, source_program_t *program)
{
	m80_context_t ctx = { .output = program };
	process(&ctx, input, path, 0);
	free(ctx.symbols);
	return !ctx.oom; /* Syntax failures are retained as diagnostic records. */
}
