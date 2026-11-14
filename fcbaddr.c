#include <ctype.h>
#include <string.h>
#include "fcbaddr.h"

static int
reg_is(const char *name, int reg)
{
	return name[0] && !name[1] && tolower((unsigned char)name[0]) == reg;
}

/* Backward substitution through a straight-line path. XCHG swaps the pair
 * whose definition is sought. Labels and other writes stop the proof. No
 * addresses are evaluated here, keeping storage/layout policy in the audit. */
int
fcb_address_before(const source_program_t *source, size_t call, fcb_address_t *address)
{
	int high = 'd', low = 'e';
	memset(address, 0, sizeof(*address));
	if (call >= source->count || source->records[call].statement.label[0]) return 0;
	while (call) {
		const source_record_t *record = &source->records[--call];
		const statement_t *stmt = &record->statement;
		const char *op = stmt->opcode;
		if (record->problem) return 0;
		if (!strcmp(op, "lxi") && reg_is(stmt->first, high)) {
			address->expression = stmt->second;
			address->definition = call;
			return 1;
		}
		if (stmt->label[0]) return 0;
		if (!*op || !strcmp(op, "nop")) continue;
		if (!strcmp(op, "xchg")) {
			high = high == 'd' ? 'h' : 'd';
			low = high == 'd' ? 'e' : 'l';
			continue;
		}
		if ((!strcmp(op, "mov") || !strcmp(op, "mvi")) &&
		    !reg_is(stmt->first, high) && !reg_is(stmt->first, low) &&
		    !reg_is(stmt->first, 'm') && !reg_is(stmt->second, 'm')) continue;
		return 0;
	}
	return 0;
}
