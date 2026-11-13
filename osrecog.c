#include <stdlib.h>
#include <string.h>

#include "osrecog.h"

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
		current = -1;
		if (!strcmp(stmt->opcode, "mvi") &&
		    (!strcmp(stmt->first, "c") || !strcmp(stmt->first, "C")))
			current = resolve(symbols, stmt->second);
	}
	*functions = values;
	return 1;
}
