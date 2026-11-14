#include <assert.h>
#include <string.h>
#include "fcbaddr.h"

int
main(void)
{
	source_record_t records[4] = {
		{ .statement = { .opcode = "lxi", .first = "H", .second = "object" } },
		{ .statement = { .opcode = "xchg" } },
		{ .statement = { .opcode = "mvi", .first = "C", .second = "15" } },
		{ .statement = { .opcode = "call", .first = "5" } }
	};
	source_program_t source = { .records = records, .count = 4 };
	fcb_address_t address;
	assert(fcb_address_before(&source, 3, &address));
	assert(address.definition == 0 && address.expression == records[0].statement.second);
	strcpy(records[1].statement.label, "alternate");
	assert(!fcb_address_before(&source, 3, &address));
	records[1].statement.label[0] = 0;
	strcpy(records[2].statement.first, "E");
	assert(!fcb_address_before(&source, 3, &address));
	return 0;
}
