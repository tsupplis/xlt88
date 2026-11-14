#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "flags.h"

int
main(void)
{
	source_record_t records[3] = {0};
	source_program_t source = { .records = records, .count = 3 };
	strcpy(records[1].statement.opcode, "push");
	strcpy(records[1].statement.first, "psw");
	strcpy(records[2].statement.opcode, "cmp");
	{
		unsigned char *live;
		assert(flags_live_plan(&source, &live));
		assert(live[0] & flag_auxiliary); /* PUSH may expose PSW. */
		free(live);
		strcpy(records[1].statement.opcode, "mov");
		assert(flags_live_plan(&source, &live));
		assert(!(live[0] & flag_auxiliary)); /* CMP overwrites AC before observation. */
		free(live);
		strcpy(records[2].statement.opcode, "daa");
		assert(flags_live_plan(&source, &live));
		assert(live[0] & flag_auxiliary);
		free(live);
	}
	{
		source_record_t branch[4] = {
			{ .statement = { .opcode = "sub", .first = "b" } },
			{ .statement = { .opcode = "jnz", .first = "observe" } },
			{ .statement = { .opcode = "cmp", .first = "b" } },
			{ .statement = { .label = "observe", .opcode = "push", .first = "psw" } }
		};
		source_program_t graph = { .records = branch, .count = 4 };
		unsigned char *live;
		assert(flags_live_plan(&graph, &live));
		assert(live[0] & flag_auxiliary); /* Taken branch observes AC, despite fallthrough CMP. */
		free(live);
		strcpy(branch[3].statement.opcode, "cmp");
		strcpy(branch[3].statement.first, "a");
		assert(flags_live_plan(&graph, &live));
		assert(!(live[0] & flag_auxiliary)); /* Both paths overwrite AC. */
		free(live);
		strcpy(branch[1].statement.first, "unknown");
		assert(flags_live_plan(&graph, &live));
		assert(live[0] & flag_auxiliary);
		free(live);
	}
	{
		source_record_t calls[5] = {
			{ .statement = { .opcode = "sub", .first = "b" } },
			{ .statement = { .opcode = "call", .first = "sink" } },
			{ .statement = { .opcode = "ret" } },
			{ .statement = { .label = "sink", .opcode = "cmp", .first = "a" } },
			{ .statement = { .opcode = "ret" } }
		};
		source_program_t graph = { .records = calls, .count = 5 };
		unsigned char *live;
		assert(flags_live_plan(&graph, &live));
		assert(live[0] == 0); /* Callee overwrites all incoming flags. */
		free(live);
		strcpy(calls[3].statement.opcode, "adc");
		assert(flags_live_plan(&graph, &live));
		assert(live[0] == flag_carry); /* Carry also affects the result value. */
		free(live);
		strcpy(calls[1].statement.opcode, "cz");
		assert(flags_live_plan(&graph, &live));
		assert(live[0] == flag_all); /* Skipped call reaches an unknown return. */
		free(live);
	}
	return 0;
}
