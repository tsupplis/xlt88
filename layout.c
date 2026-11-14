#include <stdlib.h>
#include <string.h>
#include "symbols.h"

static const char *
resolve(void *context, const char *name, int *equ, size_t *id)
{
	const symbol_t *item = symbol_find(context, name);
	if (!item) return NULL;
	*equ = item->is_equ;
	*id = item->id;
	return item->name;
}

typedef struct eval_context {
	const symbol_table_t *table;
	unsigned depth;
} eval_context_t;

static int
lookup(void *context, size_t id, const char *name, long *value)
{
	eval_context_t *eval = context;
	const symbol_t *item = id && id <= eval->table->count ?
	    &eval->table->items[id - 1] : symbol_find(eval->table, name);
	int result;
	if (!item || eval->depth >= 128) return 0;
	if (!item->is_equ) {
		if (item->source_address < 0) return 0;
		*value = item->source_address;
		return 1;
	}
	++eval->depth;
	result = expr_eval(item->expression, lookup, eval, item->definition_address, value);
	--eval->depth;
	return result;
}

int
symbols_eval(const symbol_table_t *table, const char *text, long location, long *value)
{
	eval_context_t eval = { table, 0 };
	const char *error;
	expr_t *tree = expr_parse(text, resolve, (void *)table, 0, &error);
	int result = expr_eval(tree, lookup, &eval, location, value);
	expr_destroy(tree);
	return result;
}

static long
data_size(const expr_t *tree, long width)
{
	long a, b;
	if (tree->kind == expr_list) {
		a = data_size(tree->left, width);
		b = data_size(tree->right, width);
		return a < 0 || b < 0 || a > 65536 - b ? -1 : a + b;
	}
	if (tree->kind == expr_string) {
		/* DB string length is exact. DW strings are dialect dependent;
		 * leave their layout unknown until that syntax has a contract. */
		return width == 1 ? (long)strlen(tree->text) - 2 : -1;
	}
	return width;
}

static int
in_list(const char *list, const char *opcode)
{
	size_t length = strlen(opcode);
	while (*list) {
		size_t n = strcspn(list, " ");
		if (n == length && !memcmp(list, opcode, n)) return 1;
		list += n;
		if (*list) ++list;
	}
	return 0;
}

static long
statement_size(const statement_t *stmt, const symbol_table_t *table, long pc)
{
	const char *op = stmt->opcode;
	long value;
	if (!*op || !strcmp(op, "equ") || !strcmp(op, "end")) return 0;
	if (!strcmp(op, "ds"))
		return symbols_eval(table, stmt->first, pc, &value) && value >= 0 && value <= 65536 ? value : -1;
	if (!strcmp(op, "align")) {
		if (pc < 0 || !symbols_eval(table, stmt->first, pc, &value) || value < 1 ||
		    value > 256 || (value & (value - 1))) return -1;
		return (-pc) & (value - 1);
	}
	if (!strcmp(op, "db") || !strcmp(op, "dw")) {
		const char *error;
		expr_t *tree = expr_parse(stmt->first, resolve, (void *)table, 1, &error);
		value = tree ? data_size(tree, !strcmp(op, "db") ? 1 : 2) : -1;
		expr_destroy(tree);
		return value;
	}
	if (in_list("lxi lda sta lhld shld jmp jnz jz jnc jc jpo jpe jp jm call cnz cz cnc cc cpo cpe cp cm", op)) return 3;
	if (in_list("mvi adi aci sui sbi ani xri ori cpi in out", op)) return 2;
	if (in_list("nop stax inx inr dcr rlc dad ldax dcx rrc ral rar daa cma stc cmc mov hlt add adc sub sbb ana xra ora cmp rnz pop rz ret rnc push rc rpo rpe pchl xthl xchg rp rm sphl di ei rst", op)) return 1;
	return -1;
}

static int
visit_equ(const symbol_table_t *table, const expr_t *tree, unsigned char *colors, unsigned depth)
{
	if (!tree) return 1;
	if (depth > 128) return 0;
	if (tree->kind == expr_symbol && tree->is_equ && tree->symbol_id) {
		size_t index = tree->symbol_id - 1;
		if (colors[index] == 1) return 0;
		if (colors[index] == 2) return 1;
		colors[index] = 1;
		if (!visit_equ(table, table->items[index].expression, colors, depth + 1)) return 0;
		colors[index] = 2;
	}
	return visit_equ(table, tree->left, colors, depth + 1) &&
	    visit_equ(table, tree->right, colors, depth + 1);
}

int
symbols_layout(const source_program_t *source, symbol_table_t *table, FILE *diagnostics)
{
	size_t i, pass;
	/* Bind EQU trees once; references carry IDs rather than pointers into
	 * arrays. Layout can then reevaluate forward definitions safely. */
	for (i = 0; i < table->count; ++i) {
		symbol_t *item = &table->items[i];
		item->source_address = item->definition_address = -1;
		if (item->is_equ) {
			const char *error;
			item->expression = expr_parse(item->value, resolve, table, 0, &error);
			if (!item->expression) {
				fprintf(diagnostics, "xlt88:%lu: EQU expression: %s\n", item->line, error);
				return 0;
			}
		}
	}
	{
		unsigned char *colors = calloc(table->count ? table->count : 1, 1);
		if (!colors) return 0;
		for (i = 0; i < table->count; ++i) {
			if (!visit_equ(table, table->items[i].expression, colors, 0)) {
				fprintf(diagnostics, "xlt88:%lu: cyclic or excessively deep EQU dependency\n", table->items[i].line);
				free(colors);
				return 0;
			}
		}
		free(colors);
	}
	for (pass = 0; pass <= table->count; ++pass) {
		long pc = 0x100;
		int changed = 0;
		for (i = 0; i < source->count; ++i) {
			const statement_t *stmt = &source->records[i].statement;
			const symbol_t *found = symbol_find(table, stmt->label);
			long size;
			if (!strcmp(stmt->opcode, "end")) break;
			if (found) {
				symbol_t *item = &table->items[found->id - 1];
				if (item->definition_address != pc) changed = 1;
				item->definition_address = pc;
				if (!item->is_equ) item->source_address = pc <= 65535 ? pc : -1;
			}
			if (!strcmp(stmt->opcode, "org")) {
				if (!symbols_eval(table, stmt->first, pc, &pc) || pc < 0 || pc > 65535) pc = -1;
				continue;
			}
			size = statement_size(stmt, table, pc);
			if (pc < 0 || size < 0 || size > 65536 - pc) pc = -1;
			else pc += size;
		}
		if (!changed) return 1;
	}
	/* Self-dependent layout must never export an apparently exact offset. */
	for (i = 0; i < table->count; ++i)
		table->items[i].source_address = table->items[i].definition_address = -1;
	return 1;
}
