/* Translate CP/M-80 reset through native reset/select/DMA/search operations. */
#include "reset.h"

static const char *
literal_name(void *context, const char *name, int *is_equ, size_t *id)
{
	(void)context;
	*is_equ = 0;
	*id = 0;
	return name;
}

static operand_t
literal(const char *text)
{
	return (operand_t){ .kind = operand_expression, .text = text, .resolve = literal_name };
}

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

void
reset_disks(const emitter_t *out, reset_call_t native_call, int extended_fcb,
    unsigned long site)
{
	static const char *const saved[] = {"cx", "dx", "si", "es"};
	for (size_t i = 0; i < sizeof(saved) / sizeof(*saved); ++i)
		step(out, "push", operand(operand_register, saved[i]), no_operand);
	/* CALL/POP supplies a relocatable DS offset without assembler-specific
	 * address syntax. The skipped bytes are a native FCB and search buffer. */
	branch(out, "call", "rstdata", site);
	if (extended_fcb) {
		/* Include native hidden/system files in DOS's name search. */
		emit(out, output_data_bytes, NULL, literal("255,0,0,0,0,0,6"), no_operand);
		emit(out, output_line_end, NULL, no_operand, no_operand);
	}
	emit(out, output_data_bytes, NULL, literal("0,36,63,63,63,63,63,63,63,63,63,63"), no_operand);
	emit(out, output_line_end, NULL, no_operand, no_operand);
	/* Match every CP/M extent/module, as the original reset directory
	 * scan does. DOS's native search ignores these CP/M extent fields. */
	emit(out, output_data_bytes, NULL, literal(extended_fcb ? "0,0,0" : "63,0,63"), no_operand);
	emit(out, output_line_end, NULL, no_operand, no_operand);
	emit(out, output_reserve_bytes, NULL, literal("149"), no_operand);
	emit(out, output_line_end, NULL, no_operand, no_operand);
	label(out, "rstdata", site);
	step(out, "pop", operand(operand_register, "si"), no_operand);
	native_call(out, 13);
	step(out, "mov", operand(operand_register, "dx"), value(operand_immediate, 0));
	native_call(out, 14);
	step(out, "mov", operand(operand_register, "dx"), operand(operand_register, "si"));
	step(out, "add", operand(operand_register, "dx"), value(operand_immediate, extended_fcb ? 43 : 36));
	native_call(out, 26);
	step(out, "mov", operand(operand_register, "dx"), operand(operand_register, "si"));
	native_call(out, 17);
	step(out, "cmp", operand(operand_register, "al"), value(operand_immediate, 255));
	branch(out, "je", "rstnone", site);
	step(out, "mov", operand(operand_register, "al"), value(operand_immediate, 255));
	branch(out, "jmp", "rstret", site);
	label(out, "rstnone", site);
	step(out, "mov", operand(operand_register, "al"), value(operand_immediate, 0));
	label(out, "rstret", site);
	step(out, "push", operand(operand_register, "ax"), no_operand);
	step(out, "mov", operand(operand_register, "dx"), value(operand_immediate, 128));
	native_call(out, 26);
	step(out, "pop", operand(operand_register, "ax"), no_operand);
	step(out, "mov", operand(operand_register, "bl"), operand(operand_register, "al"));
	for (size_t i = sizeof(saved) / sizeof(*saved); i; --i)
		step(out, "pop", operand(operand_register, saved[i - 1]), no_operand);
}
