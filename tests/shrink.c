#include <assert.h>
#include <string.h>
#include "target.h"

static void
fixture(int gap, int barrier, int backward, int folded)
{
	target_program_t program = {0};
	emitter_t out = target_builder(&program);
	size_t first, jump;
	if (backward) emit(&out, output_label, "dest", value(operand_immediate, 1), no_operand);
	if (backward) for (int i = 0; i < gap; ++i) instruction(&out, "nop", no_operand, no_operand);
	first = program.count;
	instruction(&out, "jz", operand(operand_internal_label, NULL), value(operand_immediate, 1));
	jump = program.count;
	instruction(&out, "jmp", operand(operand_branch, "dest"), no_operand);
	emit(&out, output_internal_definition, NULL, value(operand_immediate, 1), no_operand);
	if (barrier) emit(&out, output_alignment, NULL, value(operand_immediate, 256), no_operand);
	if (!backward) {
		for (int i = 0; i < gap; ++i) instruction(&out, "nop", no_operand, no_operand);
		emit(&out, output_label, "dest", value(operand_immediate, 1), no_operand);
	}
	assert(target_shrink(&program));
	assert(program.events[first].short_branch);
	if (folded) {
		assert(!strcmp(program.events[first].text, "jnz"));
		assert(program.events[jump].kind == output_line_end);
	} else {
		assert(!strcmp(program.events[first].text, "jz"));
		assert(program.events[jump].kind == output_instruction);
	}
	assert(target_shrink(&program)); /* A second pass remains valid. */
	target_destroy(&program);
}
int
main(void)
{
	fixture(124, 0, 0, 1);
	fixture(125, 0, 0, 0);
	fixture(122, 0, 1, 1);
	fixture(123, 0, 1, 0);
	fixture(1, 1, 0, 0);
	return 0;
}
