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

const backend_t msdos_backend = {
	.name = "msdos",
	.bdos = bdos,
	.exit_program = exit_program,
	.stack_segment = NULL
};
