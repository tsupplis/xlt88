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
	size_t i;
	long current = -1;
	long *values = malloc((program->count ? program->count : 1) * sizeof(*values));

	if (!values)
		return 0;
	for (i = 0; i < program->count; ++i) {
		const source_record_t *record = &program->records[i];
		const statement_t *stmt = &record->statement;
		if (stmt->label[0] || !strcmp(stmt->opcode, "align"))
			current = -1;
		values[i] = current;
		if (record->problem || (!stmt->label[0] && !stmt->opcode[0]))
			continue;
		if (!strcmp(stmt->opcode, "mvi") && symbol_equal(stmt->first, "c")) {
			current = resolve(symbols, stmt->second);
			if (current < 0 || current > 255) current = -1;
		} else if (!strcmp(stmt->opcode, "lxi") && symbol_equal(stmt->first, "b")) {
			long word = resolve(symbols, stmt->second);
			current = word >= 0 && word <= 65535 ? word & 255 : -1;
		} else if (one_of("mov mvi inr dcr", stmt->opcode) && symbol_equal(stmt->first, "c")) {
			current = -1;
		} else if (one_of("lxi inx dcx pop", stmt->opcode) && symbol_equal(stmt->first, "b")) {
			current = -1;
		} else if (!one_of("nop lxi stax inx inr dcr mvi rlc dad ldax dcx rrc ral rar daa cma stc cmc mov add adc sub sbb ana xra ora cmp push pop adi aci sui sbi ani xri ori cpi lda sta lhld shld xthl xchg in out di ei", stmt->opcode)) {
			current = -1;
		}
	}
	*functions = values;
	return 1;
}
