#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "flow.h"

static int
same_name(const char *a, const char *b)
{
	while (*a && *b)
		if (tolower((unsigned char)*a++) != tolower((unsigned char)*b++)) return 0;
	return *a == *b;
}

static int
condition(const char *op)
{
	static const char *const names[] = {"z", "nz", "c", "nc", "p", "m", "pe", "po"};
	for (size_t i = 0; i < sizeof(names) / sizeof(*names); ++i)
		if (!strcmp(op, names[i])) return 1;
	return 0;
}

static int
data(const char *op)
{
	return !strcmp(op, "db") || !strcmp(op, "dw") || !strcmp(op, "ds") ||
	    !strcmp(op, "equ") || !strcmp(op, "set") || !strcmp(op, "end");
}

static long
destination(const source_program_t *source, const char *name)
{
	long result = -1;
	for (size_t i = 0; i < source->count; ++i) {
		const statement_t *stmt = &source->records[i].statement;
		if (!*stmt->label || !same_name(stmt->label, name)) continue;
		if (result >= 0 || data(stmt->opcode)) return -1;
		result = (long)i;
	}
	return result;
}

static int
ordinary(const char *op)
{
	static const char *const names[] = {
		"", "mov", "mvi", "lxi", "lda", "sta", "lhld", "shld", "ldax", "stax",
		"xchg", "xthl", "sphl", "push", "pop", "nop", "di", "ei", "in", "out",
		"add", "adi", "adc", "aci", "sub", "sui", "sbb", "sbi", "cmp", "cpi",
		"ana", "ani", "xra", "xri", "ora", "ori", "inr", "dcr", "inx", "dcx",
		"dad", "daa", "cma", "stc", "cmc", "rlc", "rrc", "ral", "rar"
	};
	for (size_t i = 0; i < sizeof(names) / sizeof(*names); ++i)
		if (!strcmp(op, names[i])) return 1;
	return 0;
}

void
flow_destroy(flow_graph_t *graph)
{
	free(graph->nodes);
	memset(graph, 0, sizeof(*graph));
}

int
flow_build(const source_program_t *source, flow_graph_t *graph)
{
	graph->nodes = calloc(source->count ? source->count : 1, sizeof(*graph->nodes));
	if (!graph->nodes) return 0;
	graph->count = source->count;
	for (size_t i = 0; i < source->count; ++i) {
		const source_record_t *record = &source->records[i];
		const statement_t *stmt = &record->statement;
		const char *op = stmt->opcode;
		flow_node_t *node = &graph->nodes[i];
		node->next = i + 1 < source->count ? (long)i + 1 : -1;
		node->target = -1;
		node->conditional = *op && condition(op + 1);
		if (record->problem || data(op)) node->kind = flow_barrier;
		else if (!strcmp(op, "jmp") || (*op == 'j' && node->conditional)) node->kind = flow_branch;
		else if (!strcmp(op, "call") || (*op == 'c' && node->conditional)) node->kind = flow_call;
		else if (!strcmp(op, "ret") || (*op == 'r' && node->conditional)) node->kind = flow_return;
		else if (!strcmp(op, "pchl") || !strcmp(op, "rst")) node->kind = flow_indirect;
		else node->kind = ordinary(op) ? flow_next : flow_barrier;
		if (node->kind == flow_branch || node->kind == flow_call)
			node->target = destination(source, stmt->first);
		if ((node->kind == flow_branch || node->kind == flow_return) && !node->conditional)
			node->next = -1;
		if (node->kind == flow_indirect || node->kind == flow_barrier) node->next = -1;
	}
	return 1;
}
