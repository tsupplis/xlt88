#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "osrecog.h"
#include "symbols.h"

static int
one_of(const char *list, const char *op)
{
	size_t n = strlen(op);
	while (*list) {
		size_t length = strcspn(list, " ");
		if (n == length && !memcmp(list, op, n)) return 1;
		list += length; if (*list) ++list;
	}
	return 0;
}

int
recognize_os_calls(const source_program_t *program, resolve_value_t resolve,
    void *symbols, long **functions)
{
	os_values_t *sets = NULL;
	long *values = malloc((program->count ? program->count : 1) * sizeof(*values));
	*functions = NULL;
	if (!values || !recognize_os_sets(program, resolve, symbols, &sets)) {
		free(values);
		return 0;
	}
	for (size_t i = 0; i < program->count; ++i)
		values[i] = sets[i].count == 1 ? sets[i].values[0] : -1;
	free(sets);
	*functions = values;
	return 1;
}

/* Bounded byte-value dataflow. Unknown is absorbing at joins. Calls have
 * unknown return registers; indirect control flow invalidates label inputs. */
typedef struct os_state {
	os_values_t reg[7]; /* a,b,c,d,e,h,l */
	int reached;
} os_state_t;

static int
reg_index(const char *name)
{
	const char *p;
	if (strlen(name) != 1) return -1;
	p = strchr("abcdehl", tolower((unsigned char)*name));
	return p ? (int)(p - "abcdehl") : -1;
}

static os_values_t
constant(long value)
{
	os_values_t result = {0};
	if (value >= 0 && value <= 255) { result.count = 1; result.values[0] = (unsigned char)value; }
	return result;
}

/* Union only adds possibilities, ending at unknown when the bound is crossed.
 * Reached distinguishes absent predecessors from genuinely unknown inputs. */
static int
merge_state(os_state_t *to, const os_state_t *from)
{
	int changed = 0;
	if (!from->reached) return 0;
	if (!to->reached) { *to = *from; return 1; }
	for (int r = 0; r < 7; ++r) {
		os_values_t *a = &to->reg[r];
		const os_values_t *b = &from->reg[r];
		if (!a->count) continue;
		if (!b->count) { a->count = 0; changed = 1; continue; }
		for (int j = 0; j < b->count; ++j) {
			int k;
			for (k = 0; k < a->count && a->values[k] != b->values[j]; ++k) {}
			if (k < a->count) continue;
			changed = 1;
			if (a->count == os_value_limit) { a->count = 0; break; }
			a->values[a->count++] = b->values[j];
		}
	}
	return changed;
}

static long
branch_record(const source_program_t *program, const char *name,
    resolve_value_t resolve, void *symbols)
{
	long found = -1, address = resolve(symbols, name);
	for (size_t i = 0; i < program->count; ++i) {
		const statement_t *s = &program->records[i].statement;
		if (!s->label[0] || !strcmp(s->opcode, "equ") || !strcmp(s->opcode, "set")) continue;
		if (symbol_equal(name, s->label)) return (long)i;
		if (address >= 0 && resolve(symbols, s->label) == address) {
			if (found >= 0) return -1;
			found = (long)i;
		}
	}
	return found;
}

/* Transfer one instruction without inspecting target OS contracts. Memory
 * values and stack contents are deliberately outside this abstract domain. */
static os_state_t
transfer_state(const source_record_t *record, os_state_t out,
    resolve_value_t resolve, void *symbols)
{
	const statement_t *s = &record->statement;
	const char *op = s->opcode;
	os_state_t unknown = { .reached = 1 };
	int r = reg_index(s->first), q = reg_index(s->second);
	if (record->problem) out = unknown;
	else if (!strcmp(op, "mvi") && r >= 0) out.reg[r] = constant(resolve(symbols, s->second));
	else if (!strcmp(op, "mov") && r >= 0) out.reg[r] = q >= 0 ? out.reg[q] : constant(-1);
	else if (!strcmp(op, "lxi") && (r == 1 || r == 3 || r == 5)) {
		long v = resolve(symbols, s->second);
		out.reg[r] = constant(v >= 0 && v <= 65535 ? v >> 8 : -1);
		out.reg[r + 1] = constant(v >= 0 && v <= 65535 ? v & 255 : -1);
	} else if (one_of("inr dcr", op) && r >= 0) {
		for (int k = 0; k < out.reg[r].count; ++k)
			out.reg[r].values[k] += !strcmp(op, "inr") ? 1 : -1;
	} else if (one_of("inx dcx pop", op)) {
		if (r == 1 || r == 3 || r == 5) out.reg[r] = out.reg[r + 1] = constant(-1);
		else if (!strcmp(op, "pop")) out.reg[0] = constant(-1);
	} else if (!strcmp(op, "xchg")) {
		os_values_t t = out.reg[3];
		out.reg[3] = out.reg[5];
		out.reg[5] = t;
		t = out.reg[4];
		out.reg[4] = out.reg[6];
		out.reg[6] = t;
	} else if (one_of("lhld xthl dad", op)) out.reg[5] = out.reg[6] = constant(-1);
	else if (one_of("lda ldax in add adc sub sbb ana xra ora adi aci sui sbi ani xri ori "
	    "rlc rrc ral rar daa cma", op)) {
		out.reg[0] = !strcmp(op, "xra") && r == 0 ? constant(0) : constant(-1);
	} else if (one_of("call cz cnz cc cnc cp cm cpe cpo rst", op)) out = unknown;
	else if (!one_of("nop mov mvi lxi stax sta shld push cmp cpi stc cmc out di ei "
	    "jmp jz jnz jc jnc jp jm jpe jpo ret rz rnz rc rnc rp rm rpe rpo "
	    "pchl sphl hlt equ set", op) && *op)
		out = unknown;
	return out;
}

/* Establish direct edges and conservative roots before fixed-point iteration.
 * Native entries 0/5 have no source successor. Unknown transfers may enter
 * any labelled block, so those blocks cannot inherit a guessed input value. */
static void
seed_states(const source_program_t *program, resolve_value_t resolve,
    void *symbols, os_state_t *states, long *edges, unsigned char *incoming)
{
	size_t n = program->count;
	os_state_t unknown = { .reached = 1 };
	int indirect = 0;
	for (size_t i = 0; i < n; ++i) {
		const statement_t *s = &program->records[i].statement;
		edges[i] = -1;
		if (one_of("jmp jz jnz jc jnc jp jm jpe jpo call cz cnz cc cnc cp cm cpe cpo", s->opcode)) {
			long address = resolve(symbols, s->first);
			edges[i] = branch_record(program, s->first, resolve, symbols);
			if (edges[i] >= 0) incoming[edges[i]] = 1;
			else if (address != 0 && address != 5) indirect = 1;
		}
		if (!strcmp(s->opcode, "pchl")) indirect = 1;
		/* Translate disconnected source blocks conservatively as well. */
		if (i + 1 < n && !program->records[i + 1].statement.label[0] &&
		    one_of("jmp ret pchl hlt", s->opcode))
			states[i + 1] = unknown;
	}
	if (n) states[0] = unknown;
	/* Unreferenced labels are independent entries. Direct calls contribute
	 * their input states, but return registers are deliberately unknown. */
	for (size_t i = 0; i < n; ++i) {
		const statement_t *s = &program->records[i].statement;
		if (s->label[0] && (indirect || !incoming[i])) states[i] = unknown;
	}
}

int
recognize_os_sets(const source_program_t *program, resolve_value_t resolve,
    void *symbols, os_values_t **functions)
{
	size_t n = program->count;
	os_state_t *states = calloc(n ? n : 1, sizeof(*states));
	os_values_t *result = calloc(n ? n : 1, sizeof(*result));
	long *edges = malloc((n ? n : 1) * sizeof(*edges));
	unsigned char *incoming = calloc(n ? n : 1, 1);
	int changed = 1;

	*functions = NULL;
	if (!states || !result || !edges || !incoming) {
		free(states);
		free(result);
		free(edges);
		free(incoming);
		return 0;
	}
	seed_states(program, resolve, symbols, states, edges, incoming);
	while (changed) {
		changed = 0;
		for (size_t i = 0; i < n; ++i) {
			const source_record_t *record = &program->records[i];
			const statement_t *s = &record->statement;
			const char *op = s->opcode;
			os_state_t out;
			if (!states[i].reached) continue;
			out = transfer_state(record, states[i], resolve, symbols);
			if (edges[i] >= 0) changed |= merge_state(&states[edges[i]],
			    one_of("call cz cnz cc cnc cp cm cpe cpo", op) ? &states[i] : &out);
			if (!one_of("jmp ret pchl hlt end", op) && i + 1 < n)
				changed |= merge_state(&states[i + 1], &out);
		}
	}
	for (size_t i = 0; i < n; ++i) result[i] = states[i].reg[2];
	free(states);
	free(edges);
	free(incoming);
	*functions = result;
	return 1;
}
