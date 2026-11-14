#include <assert.h>
#include <string.h>
#include "flags.h"

int
main(void)
{
	source_record_t records[3] = {0};
	source_program_t source = { .records = records, .count = 3 };
	strcpy(records[1].statement.opcode, "mov");
	strcpy(records[2].statement.opcode, "cmp");
	assert(flags_dead_after(&source, 0));
	strcpy(records[2].statement.opcode, "adc");
	assert(!flags_dead_after(&source, 0));
	strcpy(records[2].statement.opcode, "cmp");
	strcpy(records[1].statement.label, "entry");
	assert(!flags_dead_after(&source, 0));
	records[1].statement.label[0] = 0;
	strcpy(records[1].statement.opcode, "push");
	assert(!flags_dead_after(&source, 0));
	return 0;
}
