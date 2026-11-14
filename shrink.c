#include <stdlib.h>
#include <string.h>
#include "target.h"

static const char *const conditions[] = {
	"jz", "jnz", "jc", "jnc", "jp", "jnp", "js", "jns", "jo", "jno",
	"je", "jne", "ja", "jbe", "jae", "jb", "jnae", "jnb", "jpe", "jpo"
};

static int
condition(const output_t *event)
{
	size_t i;
	if (event->kind != output_instruction) return -1;
	for (i = 0; i < sizeof(conditions)/sizeof(conditions[0]); ++i)
		if (!strcmp(event->text, conditions[i])) return (int)i;
	return -1;
}

static int
same(const char *a, const char *b)
{
	return a && b ? !strcmp(a, b) : a == b;
}

static int
matches(const output_t *branch, const output_t *label)
{
	const operand_t *destination = &branch->operands[0];
	if (destination->kind == operand_internal_label)
		return label->kind == output_internal_definition && same(destination->text, label->text) &&
		    branch->operands[1].value == label->operands[0].value;
	return destination->kind == operand_branch && destination->text &&
	    label->kind == output_label && label->operands[0].value && same(destination->text, label->text);
}

/* Upper bounds for generated 8086 instructions, shared by all renderers.
 * Memory forms reserve seven bytes (including a possible segment prefix).
 * Unknown instructions and variable-size layout events remain barriers. */
static int
bound(const output_t *event)
{
	const char *op = event->text;
	const operand_t *first = &event->operands[0], *second = &event->operands[1];
	switch (event->kind) {
	case output_instruction:
		if (condition(event) >= 0) return event->short_branch ? 2 : 5;
		{
			static const char *const known[] = {
				"mov", "add", "adc", "sub", "sbb", "cmp", "inc", "dec",
				"and", "or", "xor", "test", "xchg", "push", "pop", "pushf", "popf",
				"lahf", "sahf", "shl", "shr", "rol", "ror", "rcl", "rcr",
				"call", "jmp", "ret", "int", "in", "out", "nop", "hlt",
				"cli", "sti", "clc", "stc", "cmc", "not", "daa"
			};
			size_t i;
			for (i = 0; i < sizeof(known)/sizeof(known[0]); ++i)
				if (!strcmp(op, known[i])) break;
			if (i == sizeof(known)/sizeof(known[0])) return -1;
		}
		for (size_t i = 0; i < 2; ++i)
			if (event->operands[i].kind == operand_memory || event->operands[i].kind == operand_byte_memory) return 7;
		if (!strcmp(op, "jmp") || !strcmp(op, "call")) return 3;
		if (!strcmp(op, "mov")) return second->kind == operand_register ? 2 : 3;
		if (!strcmp(op, "push") || !strcmp(op, "pop")) return first->kind == operand_register ? 1 : 3;
		if (!strcmp(op, "inc") || !strcmp(op, "dec") || !strcmp(op, "xchg") ||
		    !strcmp(op, "not") || !strcmp(op, "int") || !strcmp(op, "in") || !strcmp(op, "out")) return 2;
		if (!strcmp(op, "shl") || !strcmp(op, "shr") || !strcmp(op, "rol") ||
		    !strcmp(op, "ror") || !strcmp(op, "rcl") || !strcmp(op, "rcr")) return 3;
		if (!strcmp(op, "add") || !strcmp(op, "adc") || !strcmp(op, "sub") ||
		    !strcmp(op, "sbb") || !strcmp(op, "cmp") || !strcmp(op, "and") ||
		    !strcmp(op, "or") || !strcmp(op, "xor") || !strcmp(op, "test"))
			return second->kind == operand_register ? 2 : 4;
		if (!strcmp(op, "ret")) return 3;
		if (!strcmp(op, "lahf") || !strcmp(op, "sahf") || !strcmp(op, "nop") ||
		    !strcmp(op, "pushf") || !strcmp(op, "popf") || !strcmp(op, "hlt") ||
		    !strcmp(op, "cli") || !strcmp(op, "sti") || !strcmp(op, "clc") ||
		    !strcmp(op, "stc") || !strcmp(op, "cmc") || !strcmp(op, "daa")) return 1;
		return -1;
	case output_comment: case output_line_end: case output_label:
	case output_internal_definition: case output_equate: return 0;
	default: return -1;
	}
}

static int
fits(const target_program_t *program, size_t from, const output_t *branch)
{
	size_t i, found = program->count, low, high;
	int distance = 0;
	for (i = 0; i < program->count; ++i) if (matches(branch, &program->events[i])) {
		if (found != program->count) return 0;
		found = i;
	}
	if (found == program->count) return 0;
	low = found < from ? found : from + 1;
	high = found < from ? from + 1 : found;
	for (i = low; i < high; ++i) {
		int size = bound(&program->events[i]);
		if (size < 0 || (distance += size) > 127) return 0;
	}
	return 1;
}

static size_t
next(const target_program_t *program, size_t index)
{
	while (++index < program->count) {
		output_kind_t kind = program->events[index].kind;
		if (kind != output_comment && kind != output_line_end) break;
	}
	return index;
}

int
target_shrink(target_program_t *program)
{
	size_t i;
	for (i = 0; i < program->count; ++i) {
		output_t *branch = &program->events[i];
		int code = condition(branch);
		size_t jump, label;
		if (code < 0) continue;
		jump = next(program, i);
		label = jump < program->count ? next(program, jump) : program->count;
		if (label < program->count && program->events[jump].kind == output_instruction &&
		    !strcmp(program->events[jump].text, "jmp") && matches(branch, &program->events[label]) &&
		    fits(program, i, &program->events[jump])) {
			output_t *transfer = &program->events[jump];
			char *name = malloc(strlen(conditions[code ^ 1]) + 1);
			if (!name) return 0;
			strcpy(name, conditions[code ^ 1]);
			free((void *)branch->text);
			branch->text = name;
			for (size_t operand = 0; operand < 2; ++operand) {
				free((void *)branch->operands[operand].text);
				expr_destroy(branch->operands[operand].expression);
				branch->operands[operand] = transfer->operands[operand];
			}
			free((void *)transfer->text);
			memset(transfer, 0, sizeof(*transfer));
			transfer->kind = output_line_end;
			branch->short_branch = 1;
		} else if (fits(program, i, branch)) {
			branch->short_branch = 1;
		}
	}
	return 1;
}
