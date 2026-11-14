/* Native CP/M-86 lowering. These hooks append ordinary 8086 instructions
 * to the target builder; assembler syntax and program storage live elsewhere.
 * Service coverage is intentionally limited to the cases listed below. */
#include "backend.h"

static void
exit_program(const emitter_t *out)
{
	instruction(out, "mov", operand(operand_register, "cl"), value(operand_immediate, 0));
	emit(out, output_comment, "CP/M-86 terminate program", no_operand, no_operand);
	emit(out, output_line_end, NULL, no_operand, no_operand);
	instruction(out, "mov", operand(operand_register, "dl"), value(operand_immediate, 0));
	emit(out, output_line_end, NULL, no_operand, no_operand);
	instruction(out, "int", value(operand_interrupt, 0xe0), no_operand);
}

static int
bdos(const emitter_t *out, long function)
{
	const char *service;

	switch (function) {
	case 1:
		service = "console input with echo";
		break;
	case 2:
		service = "console output, character in DL";
		break;
	case 9:
		service = "print dollar-terminated string at DS:DX";
		break;
	case 12:
		service = "get BDOS version";
		break;
	case 25:
		service = "get current drive";
		break;
	case 26:
		service = "set DMA offset from DX";
		break;
	default:
		return 0;
	}

	/* These services have no ES result. Keep the caller's segment. */
	instruction(out, "push", operand(operand_register, "es"), no_operand);
	emit(out, output_comment, service, no_operand, no_operand);
	emit(out, output_line_end, NULL, no_operand, no_operand);
	instruction(out, "int", value(operand_interrupt, 0xe0), no_operand);
	emit(out, output_line_end, NULL, no_operand, no_operand);
	instruction(out, "pop", operand(operand_register, "es"), no_operand);
	return 1;
}

static void
stack_segment(const emitter_t *out)
{
	/* The loader can leave SS pointing into the CCP. An 8080 program
	 * that chooses SP expects its stack in the data segment. MOV SS
	 * defers interrupts through the following MOV SP. */
	instruction(out, "mov", operand(operand_register, "si"), operand(operand_register, "ds"));
	emit(out, output_comment, "select program stack segment", no_operand, no_operand);
	emit(out, output_line_end, NULL, no_operand, no_operand);
	instruction(out, "mov", operand(operand_register, "ss"), operand(operand_register, "si"));
	emit(out, output_line_end, NULL, no_operand, no_operand);
}

const backend_t cpm86_backend = {
	.name = "cpm86",
	.bdos = bdos,
	.exit_program = exit_program,
	.stack_segment = stack_segment
};
