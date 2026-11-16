#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "flags.h"
#include "flow.h"

static void
call_cases(void)
{
	source_record_t records[] = {
		{ .statement = { .opcode = "sub", .first = "b" } },
		{ .statement = { .opcode = "call", .first = "leaf" } },
		{ .statement = { .opcode = "cmp", .first = "a" } },
		{ .statement = { .opcode = "sub", .first = "b" } },
		{ .statement = { .opcode = "call", .first = "leaf" } },
		{ .statement = { .opcode = "push", .first = "psw" } },
		{ .statement = { .opcode = "ret" } },
		{ .statement = { .label = "leaf", .opcode = "mov", .first = "b", .second = "c" } },
		{ .statement = { .opcode = "ret" } }
	};
	source_program_t source = { .records = records, .count = sizeof(records) / sizeof(*records) };
	unsigned char *live;
	assert(flags_live_plan(&source, &live));
	assert(live[0] == 0); /* This continuation replaces flags. */
	assert(live[3] == flag_all); /* The same leaf must preserve flags here. */
	assert(live[7] == flag_all); /* Do not optimize the shared body globally. */
	free(live);
	strcpy(records[7].statement.opcode, "rnz");
	assert(flags_live_plan(&source, &live));
	assert(live[0] == flag_zero); /* Conditional return observes Z first. */
	free(live);
	strcpy(records[7].statement.opcode, "daa");
	assert(flags_live_plan(&source, &live));
	assert(live[0] == (flag_carry | flag_auxiliary));
	free(live);
	strcpy(records[7].statement.opcode, "mov");
	strcpy(records[7].statement.first, "m");
	assert(flags_live_plan(&source, &live));
	assert(live[0] == flag_all); /* A store might overwrite the return address. */
	free(live);
	strcpy(records[7].statement.opcode, "lxi");
	strcpy(records[7].statement.first, "sp");
	assert(flags_live_plan(&source, &live));
	assert(live[0] == flag_all);
	free(live);
	strcpy(records[7].statement.opcode, "push");
	strcpy(records[7].statement.first, "b");
	assert(flags_live_plan(&source, &live));
	assert(live[0] == flag_all); /* Unmatched stack data is not a return edge. */
	free(live);
	strcpy(records[7].statement.opcode, "jmp");
	strcpy(records[7].statement.first, "tail");
	strcpy(records[8].statement.label, "tail");
	assert(flags_live_plan(&source, &live));
	assert(live[0] == 0); /* Direct tail transfer retains this call's stack. */
	free(live);
	strcpy(records[1].statement.opcode, "cz");
	assert(flags_live_plan(&source, &live));
	assert(live[0] == flag_zero); /* The call's own condition still observes Z. */
	free(live);
	strcpy(records[1].statement.opcode, "call");
	strcpy(records[7].statement.first, "unresolved");
	assert(flags_live_plan(&source, &live));
	assert(live[0] == flag_all);
	free(live);
	strcpy(records[7].statement.opcode, "call");
	strcpy(records[7].statement.first, "leaf");
	assert(flags_live_plan(&source, &live));
	assert(live[0] == flag_all); /* Recursion is outside this leaf proof. */
	free(live);
}

static void
flow_cases(void)
{
	source_record_t records[] = {
		{ .statement = { .opcode = "call", .first = "inner" } },
		{ .statement = { .opcode = "cmp", .first = "a" } },
		{ .statement = { .opcode = "call", .first = "outer" } },
		{ .statement = { .opcode = "push", .first = "psw" } },
		{ .statement = { .opcode = "jmp", .first = "0" } },
		{ .statement = { .label = "outer", .opcode = "mvi", .first = "a", .second = "16" } },
		{ .statement = { .label = "inner", .opcode = "sui", .first = "1" } },
		{ .statement = { .opcode = "rnz" } },
		{ .statement = { .opcode = "ret" } },
		{ .statement = { .label = "vector", .opcode = "dw", .first = "inner" } }
	};
	source_program_t source = { .records = records, .count = sizeof(records) / sizeof(*records) };
	flow_graph_t graph = {0};
	unsigned char *live;
	assert(flow_build(&source, &graph));
	assert(graph.nodes[0].kind == flow_call && graph.nodes[0].target == 6 && graph.nodes[0].next == 1);
	assert(graph.nodes[2].target == 5);
	assert(graph.nodes[5].next == 6); /* Labels do not partition subroutines. */
	assert(graph.nodes[7].kind == flow_return && graph.nodes[7].conditional);
	assert(graph.nodes[7].target == -1 && graph.nodes[7].next == 8);
	assert(graph.nodes[8].kind == flow_return && graph.nodes[8].next == -1);
	assert(graph.nodes[4].kind == flow_branch && graph.nodes[4].target == -1);
	assert(graph.nodes[9].kind == flow_barrier && graph.nodes[9].next == -1);
	flow_destroy(&graph);
	assert(flags_live_plan(&source, &live));
	assert(live[6] & flag_auxiliary); /* The fall-through caller observes PSW. */
	free(live);
	strcpy(records[0].statement.first, "vector");
	strcpy(records[1].statement.opcode, "pchl");
	strcpy(records[2].statement.opcode, "cnz");
	assert(flow_build(&source, &graph));
	assert(graph.nodes[0].target == -1); /* Data labels are not code targets. */
	assert(graph.nodes[1].kind == flow_indirect && graph.nodes[1].next == -1);
	assert(graph.nodes[2].conditional && graph.nodes[2].next == 3);
	flow_destroy(&graph);
	strcpy(records[0].statement.first, "inner");
	strcpy(records[9].statement.label, "INNER");
	assert(flow_build(&source, &graph));
	assert(graph.nodes[0].target == -1); /* Duplicate/case-folded names are ambiguous. */
	flow_destroy(&graph);
}

int
main(void)
{
	flow_cases();
	call_cases();
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
