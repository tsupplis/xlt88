#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "flags.h"

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

/* Resolve only unique literal labels. Expressions and indirect destinations
 * remain barriers rather than depending on a particular symbol-table owner. */
static long
jump_record(const source_program_t *source, const char *name)
{
	long found = -1;
	for (size_t i = 0; i < source->count; ++i) {
		const statement_t *stmt = &source->records[i].statement;
		if (!stmt->label[0] || !same_name(stmt->label, name)) continue;
		if (found >= 0 || !strcmp(stmt->opcode, "equ")) return -1;
		found = (long)i;
	}
	return found;
}

int
flags_live_plan(const source_program_t *source, unsigned char **after)
{
	static const char *const write[] = {
		"add", "adi", "adc", "aci", "sub", "sui", "sbb", "sbi",
		"cmp", "cpi", "ana", "ani", "xra", "xri", "ora", "ori", "inr", "dcr"
	};
	static const char *const preserve[] = {
		"mov", "mvi", "lxi", "lda", "sta", "lhld", "shld", "ldax", "stax",
		"xchg", "xthl", "nop", "sphl", "cma", "di", "ei", "in", "out", "inx", "dcx", "dad", "rlc", "rrc", "ral", "rar", "stc", "cmc"
	};
	static const char *const branch[] = {"jmp", "jz", "jnz", "jc", "jnc", "jp", "jm", "jpe", "jpo"};
	static const char *const calls[] = {"call", "cz", "cnz", "cc", "cnc", "cp", "cm", "cpe", "cpo"};
	size_t count = source->count;
	unsigned char *before = malloc(count ? count : 1);
	unsigned char *result = malloc(count ? count : 1);
	long *edges = malloc((count ? count : 1) * sizeof(*edges));
	int changed;

	*after = NULL;
	if (!before || !result || !edges) {
		free(before);
		free(result);
		free(edges);
		return 0;
	}
	memset(before, flag_all, count);
	for (size_t i = 0; i < count; ++i)
		edges[i] = (listed(source->records[i].statement.opcode, branch, sizeof(branch)/sizeof(*branch)) ||
		    listed(source->records[i].statement.opcode, calls, sizeof(calls)/sizeof(*calls))) ?
		    jump_record(source, source->records[i].statement.first) : -1;
	/* Start conservatively live. A flag becomes dead only once every
	 * successor proves it overwritten before observation; loops converge. */
	do {
		changed = 0;
		for (size_t i = count; i-- > 0;) {
			const source_record_t *record = &source->records[i];
			const statement_t *stmt = &record->statement;
			const char *op = stmt->opcode;
			unsigned char live = i + 1 < count ? before[i + 1] : flag_all;
			int is_branch = listed(op, branch, sizeof(branch)/sizeof(*branch));
			if (is_branch) {
				unsigned char taken = edges[i] >= 0 ? before[edges[i]] : flag_all;
				live = !strcmp(op, "jmp") ? taken : live | taken;
			}
			result[i] = live;
			if (record->problem) live = flag_all;
			else if (listed(op, calls, sizeof(calls)/sizeof(*calls))) {
				/* A direct callee may overwrite flags before reading them. RET
				 * stays live, so this never assumes preserved return flags.
				 * Conditional calls must also account for the skipped path. */
				unsigned char entry = edges[i] >= 0 ? before[edges[i]] : flag_all;
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
			if (is_branch || listed(op, calls, sizeof(calls)/sizeof(*calls))) {
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
	free(edges);
	*after = result;
	return 1;
}
