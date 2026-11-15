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
/* Flag-image sharing must stop before any observer or AH modification. */
static void
flag_fixture(const char *second, int removed)
{
	target_program_t program = {0};
	emitter_t out = target_builder(&program);
	instruction(&out, "lahf", no_operand, no_operand);
	instruction(&out, "inc", operand(operand_register, "bx"), no_operand);
	instruction(&out, "sahf", no_operand, no_operand);
	instruction(&out, "lahf", no_operand, no_operand);
	instruction(&out, second, operand(operand_register, "cx"), no_operand);
	instruction(&out, "sahf", no_operand, no_operand);
	assert(target_shrink(&program));
	assert((program.events[2].kind == output_line_end) == removed);
	assert((program.events[3].kind == output_line_end) == removed);
	assert(target_shrink(&program));
	target_destroy(&program);
}

static void
image_fixture(const char *op, int mask, int label, int removed)
{
	target_program_t program = {0};
	emitter_t out = target_builder(&program);
	size_t read;
	instruction(&out, "lahf", no_operand, no_operand);
	instruction(&out, op, operand(operand_register, "ah"), value(operand_immediate, mask));
	instruction(&out, "sahf", no_operand, no_operand);
	if (label) emit(&out, output_label, "entry", value(operand_immediate, 1), no_operand);
	read = program.count;
	instruction(&out, "lahf", no_operand, no_operand);
	assert(target_shrink(&program));
	assert((program.events[read].kind == output_line_end) == removed);
	assert(target_shrink(&program));
	target_destroy(&program);
}

static void
restore_fixture(const char *op, const char *reg, int label, int removed)
{
	target_program_t program = {0};
	emitter_t out = target_builder(&program);
	instruction(&out, "sahf", no_operand, no_operand);
	instruction(&out, op, operand(operand_register, reg), no_operand);
	if (label) emit(&out, output_label, "entry", value(operand_immediate, 1), no_operand);
	instruction(&out, "sahf", no_operand, no_operand);
	assert(target_shrink(&program));
	assert((program.events[0].kind == output_line_end) == removed);
	assert(program.events[program.count - 1].kind == output_instruction);
	assert(target_shrink(&program));
	target_destroy(&program);
}

int
main(void)
{
	restore_fixture("inc", "bx", 0, 1);
	restore_fixture("dec", "dx", 0, 1);
	restore_fixture("inc", "ax", 0, 0);
	restore_fixture("inc", "ah", 0, 0);
	restore_fixture("rcl", "bx", 0, 0);
	restore_fixture("inc", "bx", 1, 0);
	image_fixture("xor", 16, 0, 1);
	image_fixture("or", 16, 0, 1);
	image_fixture("and", 239, 0, 1);
	image_fixture("xor", 2, 0, 0);
	image_fixture("or", 8, 0, 0);
	image_fixture("and", 253, 0, 0);
	image_fixture("mov", 16, 0, 0);
	image_fixture("xor", 16, 1, 0);
	flag_fixture("inc", 1);
	flag_fixture("push", 0);
	fixture(124, 0, 0, 1);
	fixture(125, 0, 0, 0);
	fixture(122, 0, 1, 1);
	fixture(123, 0, 1, 0);
	fixture(1, 1, 0, 0);
	return 0;
}
