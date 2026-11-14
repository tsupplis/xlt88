/* Adapt native DOS 0Ah buffer representation; editing remains DOS policy.
 * A private buffer keeps DOS's CR outside the caller's max+2 allocation. */
#include "conbuf.h"

static void
step(const emitter_t *out, const char *op, operand_t a, operand_t b)
{
	instruction(out, op, a, b);
	emit(out, output_line_end, NULL, no_operand, no_operand);
}

static void
branch(const emitter_t *out, const char *op, const char *name, unsigned long site)
{
	step(out, op, operand(operand_internal_label, name), value(operand_immediate, site));
}

static void
label(const emitter_t *out, const char *name, unsigned long site)
{
	emit(out, output_internal_definition, name, value(operand_immediate, site), no_operand);
}

#define reg(name) operand(operand_register, name)
#define mem(name) operand(operand_memory, name)
#define imm(n) value(operand_immediate, n)

void
dos_console_buffer(const emitter_t *out, unsigned long site)
{
	static const char *const saved[] = {"ax", "bx", "cx", "dx", "si", "di", "es"};
	for (size_t i = 0; i < sizeof(saved) / sizeof(*saved); ++i)
		step(out, "push", reg(saved[i]), no_operand);
	step(out, "mov", reg("bx"), reg("dx"));
	step(out, "mov", reg("al"), mem("bx"));
	step(out, "cmp", reg("al"), imm(255));
	branch(out, "je", "cb_bad", site);
	step(out, "or", reg("al"), reg("al"));
	branch(out, "je", "cb_empty", site);
	branch(out, "call", "cb_data", site);
	emit(out, output_reserve_bytes, NULL, operand(operand_expression, "257"), no_operand);
	emit(out, output_line_end, NULL, no_operand, no_operand);
	label(out, "cb_data", site);
	step(out, "pop", reg("si"), no_operand);
	step(out, "inc", reg("al"), no_operand);
	step(out, "mov", mem("si"), reg("al"));
	step(out, "inc", reg("si"), no_operand);
	step(out, "xor", reg("ax"), reg("ax"));
	step(out, "mov", mem("si"), reg("al")); /* disable recall template */
	step(out, "dec", reg("si"), no_operand);
	step(out, "mov", reg("dx"), reg("si"));
	step(out, "push", reg("bx"), no_operand);
	step(out, "push", reg("si"), no_operand);
	step(out, "mov", reg("ah"), imm(10));
	step(out, "int", value(operand_interrupt, 0x21), no_operand);
	step(out, "pop", reg("si"), no_operand);
	step(out, "pop", reg("bx"), no_operand);
	step(out, "inc", reg("si"), no_operand);
	step(out, "mov", reg("al"), mem("si"));
	/* Defend the caller's bound even if native DOS returns an excessive count. */
	step(out, "cmp", reg("al"), mem("bx"));
	branch(out, "jbe", "cb_count", site);
	step(out, "mov", reg("al"), mem("bx"));
	label(out, "cb_count", site);
	step(out, "inc", reg("bx"), no_operand);
	step(out, "mov", mem("bx"), reg("al"));
	step(out, "xor", reg("cx"), reg("cx"));
	step(out, "mov", reg("cl"), reg("al"));
	branch(out, "jcxz", "cb_done", site);
	label(out, "cb_copy", site);
	step(out, "inc", reg("si"), no_operand);
	step(out, "inc", reg("bx"), no_operand);
	step(out, "mov", reg("al"), mem("si"));
	step(out, "mov", mem("bx"), reg("al"));
	step(out, "dec", reg("cx"), no_operand);
	branch(out, "jnz", "cb_copy", site);
	branch(out, "jmp", "cb_done", site);
	label(out, "cb_empty", site);
	step(out, "inc", reg("bx"), no_operand);
	step(out, "mov", mem("bx"), reg("al"));
	label(out, "cb_done", site);
	for (size_t i = sizeof(saved) / sizeof(*saved); i; --i)
		step(out, "pop", reg(saved[i - 1]), no_operand);
	branch(out, "jmp", "cb_end", site);
	label(out, "cb_bad", site);
	branch(out, "call", "cb_error", site);
	emit(out, output_data_bytes, NULL,
	    operand(operand_expression, "13,10,'DOS input capacity 255 needs a native port',13,10,'$'"), no_operand);
	emit(out, output_line_end, NULL, no_operand, no_operand);
	label(out, "cb_error", site);
	step(out, "pop", reg("dx"), no_operand);
	step(out, "mov", reg("ah"), imm(9));
	step(out, "int", value(operand_interrupt, 0x21), no_operand);
	step(out, "int", value(operand_interrupt, 0x20), no_operand);
	label(out, "cb_end", site);
}
