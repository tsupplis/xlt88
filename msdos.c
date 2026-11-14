/* Native DOS 1.0 lowering through the shared target builder interface. */
#include "backend.h"

static void
exit_program(const emitter_t *out)
{
	/* DOS 1.0 loads a COM file with CS pointing to its PSP. */
	instruction(out, "int", value(operand_interrupt, 0x20), no_operand);
	emit(out, output_comment, "terminate DOS COM program", no_operand, no_operand);
}

static int
bdos(const emitter_t *out, long function)
{
	int returns_byte = 0;
	const char *service;

	switch (function) {
	case 1:
		returns_byte = 1;
		service = "DOS console input with echo";
		break;
	case 2:
		service = "DOS console output, character in DL";
		break;
	case 9:
		service = "DOS print dollar-terminated string at DS:DX";
		break;
	case 11:
		returns_byte = 1;
		service = "DOS console input status";
		break;
	case 25:
		returns_byte = 1;
		service = "DOS get current drive";
		break;
	case 26:
		service = "DOS set DTA from DS:DX";
		break;
	default:
		return 0;
	}
	instruction(out, "mov", operand(operand_register, "ah"), value(operand_immediate, function));
	emit(out, output_comment, service, no_operand, no_operand);
	emit(out, output_line_end, NULL, no_operand, no_operand);
	instruction(out, "int", value(operand_interrupt, 0x21), no_operand);
	if (returns_byte) {
		emit(out, output_line_end, NULL, no_operand, no_operand);
		instruction(out, "mov", operand(operand_register, "bl"), operand(operand_register, "al"));
		emit(out, output_comment, "BDOS byte result in A and L", no_operand, no_operand);
	}
	return 1;
}

static void
file_int(const emitter_t *out, long function)
{
	instruction(out, "mov", operand(operand_register, "ah"), value(operand_immediate, function));
	emit(out, output_line_end, NULL, no_operand, no_operand);
	instruction(out, "int", value(operand_interrupt, 0x21), no_operand);
	emit(out, output_line_end, NULL, no_operand, no_operand);
}

static void
file_branch(const emitter_t *out, const char *op, const char *prefix, unsigned long site)
{
	instruction(out, op, operand(operand_internal_label, prefix), value(operand_immediate, site));
	emit(out, output_line_end, NULL, no_operand, no_operand);
}

static void
file_label(const emitter_t *out, const char *prefix, unsigned long site)
{
	emit(out, output_internal_definition, prefix, value(operand_immediate, site), no_operand);
}

static int
file_call(const emitter_t *out, long function, unsigned long site)
{
	if (function != 15 && function != 16 && function != 20 && function != 21 && function != 22) return 0;
	emit(out, output_comment, "statically translated CP/M FCB with opaque metadata", no_operand, no_operand);
	emit(out, output_line_end, NULL, no_operand, no_operand);
	if (function == 22) {
		/* Never silently replace an existing file with DOS create. The
		 * audited source cannot inspect metadata modified by this probe. */
		file_int(out, 15);
		instruction(out, "cmp", operand(operand_register, "al"), value(operand_immediate, 255));
		emit(out, output_line_end, NULL, no_operand, no_operand);
		file_branch(out, "je", "make", site);
		file_int(out, 16);
		instruction(out, "mov", operand(operand_register, "al"), value(operand_immediate, 255));
		emit(out, output_line_end, NULL, no_operand, no_operand);
		file_branch(out, "jmp", "fileret", site);
		file_label(out, "make", site);
	}
	file_int(out, function);
	if (function == 20) {
		instruction(out, "cmp", operand(operand_register, "al"), value(operand_immediate, 3));
		emit(out, output_line_end, NULL, no_operand, no_operand);
		file_branch(out, "jne", "fileret", site);
		instruction(out, "mov", operand(operand_register, "al"), value(operand_immediate, 0));
		emit(out, output_line_end, NULL, no_operand, no_operand);
	}
	file_label(out, "fileret", site);
	instruction(out, "mov", operand(operand_register, "bl"), operand(operand_register, "al"));
	return 1;
}

const backend_t msdos_backend = {
	.name = "msdos",
	.bdos = bdos,
	.exit_program = exit_program,
	.stack_segment = NULL,
	.file_call = file_call
};
