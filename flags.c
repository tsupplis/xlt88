#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "flags.h"
#include "flow.h"

static int
listed(const char *op, const char *const *names, size_t count)
{
	for (size_t i = 0; i < count; ++i)
		if (!strcmp(op, names[i])) return 1;
	return 0;
}

static int
same_name(const char *a, const char *b)
{
	while (*a && *b) {
		if (tolower((unsigned char)*a++) != tolower((unsigned char)*b++)) return 0;
	}
	return *a == *b;
}

static const char *const write[] = {
	"add", "adi", "adc", "aci", "sub", "sui", "sbb", "sbi",
	"cmp", "cpi", "ana", "ani", "xra", "xri", "ora", "ori", "inr", "dcr"
};
static const char *const preserve[] = {
	"mov", "mvi", "lxi", "lda", "sta", "lhld", "shld", "ldax", "stax",
	"xchg", "xthl", "nop", "sphl", "cma", "di", "ei", "in", "out", "inx", "dcx", "dad", "rlc", "rrc", "ral", "rar", "stc", "cmc"
};

typedef struct flag_summary {
	unsigned char known, read, pass;
} flag_summary_t;

/* Prove a call's return stack untouched. Memory writes might alias the saved
 * return address; even balanced PUSH/POP and nested calls are left for a
 * stronger stack analysis. Multiple labels and direct tail branches are fine. */
static int
leaf_step(const statement_t *stmt)
{
	const char *op = stmt->opcode;
	if (!strcmp(op, "push") || !strcmp(op, "pop") || !strcmp(op, "xthl") ||
	    !strcmp(op, "sphl") || !strcmp(op, "sta") || !strcmp(op, "shld") ||
	    !strcmp(op, "stax")) return 0;
	if ((!strcmp(op, "mov") || !strcmp(op, "mvi") || !strcmp(op, "inr") ||
	     !strcmp(op, "dcr")) && same_name(stmt->first, "m")) return 0;
	if ((!strcmp(op, "lxi") || !strcmp(op, "inx") || !strcmp(op, "dcx")) &&
	    same_name(stmt->first, "sp")) return 0;
	return 1;
}

static unsigned char
tested_flag(const char *op)
{
	const char *condition = op + 1;
	if (!strcmp(condition, "z") || !strcmp(condition, "nz")) return flag_zero;
	if (!strcmp(condition, "c") || !strcmp(condition, "nc")) return flag_carry;
	if (!strcmp(condition, "m") || !strcmp(condition, "p")) return flag_sign;
	if (!strcmp(condition, "pe") || !strcmp(condition, "po")) return flag_parity;
	return 0;
}

/* Monotone summaries describe reads of incoming flags and flags passed back
 * unchanged. Start fully live; unresolved successors poison the proof. Loops
 * converge conservatively, without assuming they terminate. */
static void
leaf_summaries(const source_program_t *source, const flow_graph_t *graph,
    flag_summary_t *summary)
{
	int changed;
	for (size_t i = 0; i < source->count; ++i)
		summary[i] = (flag_summary_t){1, flag_all, flag_all};
	do {
		changed = 0;
		for (size_t i = source->count; i-- > 0;) {
			const flow_node_t *node = &graph->nodes[i];
			const statement_t *stmt = &source->records[i].statement;
			const char *op = stmt->opcode;
			flag_summary_t value = {0};
			unsigned char use = 0, kill = 0;
			if (node->kind == flow_return) value = (flag_summary_t){1, 0, flag_all};
			else if (node->kind == flow_branch && node->target >= 0) value = summary[node->target];
			else if (node->kind == flow_next && leaf_step(stmt) && node->next >= 0) value = summary[node->next];
			if ((node->kind == flow_return || node->kind == flow_branch) && node->conditional) {
				flag_summary_t skipped = node->next >= 0 ? summary[node->next] : (flag_summary_t){0};
				value.known &= skipped.known;
				value.read |= skipped.read;
				value.pass |= skipped.pass;
				use = tested_flag(op);
			}
			if (listed(op, write, sizeof(write) / sizeof(*write))) kill = flag_all;
			if (!strcmp(op, "inr") || !strcmp(op, "dcr")) kill &= ~flag_carry;
			if (!strcmp(op, "adc") || !strcmp(op, "aci") || !strcmp(op, "sbb") || !strcmp(op, "sbi")) use |= flag_carry;
			if (!strcmp(op, "daa")) { use |= flag_carry | flag_auxiliary; kill = flag_all; }
			if (!strcmp(op, "dad") || !strcmp(op, "stc") || !strcmp(op, "rlc") || !strcmp(op, "rrc") ||
			    !strcmp(op, "ral") || !strcmp(op, "rar") || !strcmp(op, "cmc")) kill = flag_carry;
			if (!strcmp(op, "ral") || !strcmp(op, "rar") || !strcmp(op, "cmc")) use |= flag_carry;
			value.read = use | (value.read & ~kill);
			value.pass &= ~kill;
			if (!value.known) value = (flag_summary_t){0, flag_all, flag_all};
			if (value.known != summary[i].known || value.read != summary[i].read || value.pass != summary[i].pass) {
				summary[i] = value;
				changed = 1;
			}
		}
	} while (changed);
}

int
flags_live_plan(const source_program_t *source, unsigned char **after)
{
	size_t count = source->count;
	unsigned char *before = malloc(count ? count : 1);
	unsigned char *result = malloc(count ? count : 1);
	flag_summary_t *summary = calloc(count ? count : 1, sizeof(*summary));
	flow_graph_t graph = {0};
	int changed;

	*after = NULL;
	if (!before || !result || !summary || !flow_build(source, &graph)) {
		free(before);
		free(result);
		free(summary);
		flow_destroy(&graph);
		return 0;
	}
	memset(before, flag_all, count);
	leaf_summaries(source, &graph, summary);
	/* Start conservatively live. A flag becomes dead only once every
	 * successor proves it overwritten before observation; loops converge. */
	do {
		changed = 0;
		for (size_t i = count; i-- > 0;) {
			const source_record_t *record = &source->records[i];
			const statement_t *stmt = &record->statement;
			const char *op = stmt->opcode;
			const flow_node_t *node = &graph.nodes[i];
			unsigned char live = node->next >= 0 ? before[node->next] : flag_all;
			int is_branch = node->kind == flow_branch;
			if (is_branch) {
				unsigned char taken = node->target >= 0 ? before[node->target] : flag_all;
				live = !strcmp(op, "jmp") ? taken : live | taken;
			}
			result[i] = live;
			if (record->problem || node->kind == flow_barrier) live = flag_all;
			else if (node->kind == flow_call) {
				/* A proved leaf uses this call's continuation. Otherwise retain
				 * the shared-entry plan, whose RET keeps every flag live.
				 * Conditional calls also account for their skipped path. */
				unsigned char entry = node->target >= 0 ? before[node->target] : flag_all;
				if (node->target >= 0 && summary[node->target].known)
					entry = summary[node->target].read | (live & summary[node->target].pass);
				live = !strcmp(op, "call") ? entry : live | entry;
			}
			else if (!strcmp(op, "inr") || !strcmp(op, "dcr")) live &= flag_carry;
			else if (!strcmp(op, "adc") || !strcmp(op, "aci") || !strcmp(op, "sbb") || !strcmp(op, "sbi")) live = flag_carry;
			else if (!strcmp(op, "daa")) live = flag_carry | flag_auxiliary;
			else if (listed(op, write, sizeof(write)/sizeof(*write))) live = 0;
			else if (!strcmp(op, "dad") || !strcmp(op, "stc") || !strcmp(op, "rlc") || !strcmp(op, "rrc")) live &= ~flag_carry;
			else if (!strcmp(op, "ral") || !strcmp(op, "rar") || !strcmp(op, "cmc")) live |= flag_carry;
			else if (!*op || is_branch || listed(op, preserve, sizeof(preserve)/sizeof(*preserve))) {}
			else if (!strcmp(op, "pop") && same_name(stmt->first, "psw")) live = 0;
			else if ((!strcmp(op, "push") || !strcmp(op, "pop")) && !same_name(stmt->first, "psw")) {}
			else live = flag_all; /* PSW reads, returns and unknown syntax. */
			if (is_branch || node->kind == flow_call) {
				/* Conditional transfers observe their tested flag even if
				 * both successors overwrite it immediately. */
				const char *condition = op + 1;
				if (!strcmp(condition, "z") || !strcmp(condition, "nz")) live |= flag_zero;
				else if (!strcmp(condition, "c") || !strcmp(condition, "nc")) live |= flag_carry;
				else if (!strcmp(condition, "m") || !strcmp(condition, "p")) live |= flag_sign;
				else if (!strcmp(condition, "pe") || !strcmp(condition, "po")) live |= flag_parity;
			}
			if (before[i] != live) { before[i] = live; changed = 1; }
		}
	} while (changed);
	free(before);
	free(summary);
	flow_destroy(&graph);
	*after = result;
	return 1;
}
