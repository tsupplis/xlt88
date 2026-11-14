#include <string.h>
#include "flags.h"

static int
listed(const char *op, const char *const *names, size_t count)
{
	for (size_t i = 0; i < count; ++i)
		if (!strcmp(op, names[i])) return 1;
	return 0;
}

int
flags_dead_after(const source_program_t *source, size_t index)
{
	static const char *const overwrite[] = {
		"add", "adi", "sub", "sui", "cmp", "cpi",
		"ana", "ani", "xra", "xri", "ora", "ori"
	};
	static const char *const transparent[] = {
		"mov", "mvi", "lxi", "lda", "sta", "lhld", "shld",
		"ldax", "stax", "xchg", "nop", "inx", "dcx"
	};
	while (++index < source->count) {
		const source_record_t *record = &source->records[index];
		const statement_t *stmt = &record->statement;
		/* Labels, calls, stack effects and unknown operations are barriers.
		 * In particular ADC/SBB/DAA and PSW accesses may read old flags. */
		if (record->problem || stmt->label[0]) return 0;
		if (!stmt->opcode[0]) continue;
		if (listed(stmt->opcode, overwrite, sizeof(overwrite)/sizeof(*overwrite))) return 1;
		if (!listed(stmt->opcode, transparent, sizeof(transparent)/sizeof(*transparent))) return 0;
	}
	return 0;
}
