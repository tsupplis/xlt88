/* Native CP/M-86 lowering. These hooks append ordinary 8086 instructions
 * to the target builder; assembler syntax and program storage live elsewhere.
 * The documented CP/M 2.2 entry set is lowered below; native structures and
 * unverified device/state behavior retain the contracts recorded in api.md. */
#include "backend.h"
#include "reset.h"

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

static void
native_bdos(const emitter_t *out)
{
	instruction(out, "int", value(operand_interrupt, 0xe0), no_operand);
}

static void
reset_call(const emitter_t *out, long function)
{
	instruction(out, "mov", operand(operand_register, "cl"), value(operand_immediate, function));
	emit(out, output_line_end, NULL, no_operand, no_operand);
	instruction(out, "int", value(operand_interrupt, 0xe0), no_operand);
	emit(out, output_line_end, NULL, no_operand, no_operand);
}

static int
shared_bdos(const emitter_t *out, const unsigned char *functions, size_t count)
{
	if (!count) return 0;
	for (size_t i = 0; i < count; ++i) {
		unsigned int f = functions[i];
		/* Only identical native INT bodies with ES preservation. Reset and
		 * direct I/O adapt semantics; far pointers need ES and diagnostics. */
		if (f < 1 || f > 40 || f == 6 || f == 13 || f == 27 || f == 31 ||
		    f == 38 || f == 39) return 0;
	}
	if (!out) return 1;
	instruction(out, "push", operand(operand_register, "es"), no_operand);
	emit(out, output_comment, "shared native BDOS lowering; CL selects service", no_operand, no_operand);
	emit(out, output_line_end, NULL, no_operand, no_operand);
	native_bdos(out);
	emit(out, output_line_end, NULL, no_operand, no_operand);
	instruction(out, "pop", operand(operand_register, "es"), no_operand);
	return 1;
}

static int
bdos(const emitter_t *out, long function, unsigned long site)
{
	const char *service;
	if (function == 13) { reset_disks(out, reset_call, 0, site); return 1; }

	switch (function) {
	case 1:
		service = "console input with echo";
		break;
	case 2:
		service = "console output, character in DL";
		break;
	case 3:
		service = "native reader/auxiliary input";
		break;
	case 4:
		service = "native auxiliary output, character in DL";
		break;
	case 5:
		service = "native list output, character in DL";
		break;
	case 6: 
		service = "direct console I/O, CP/M 2.2 modes"; 
		break;
	case 7:
		service = "get native BIOS IOBYTE";
		break;
	case 8:
		service = "set native BIOS IOBYTE from DL";
		break;
	case 9:
		service = "print dollar-terminated string at DS:DX";
		break;
	case 10: 
		service = "read CP/M console buffer at DS:DX"; 
		break;
	case 11: 
		service = "console input status"; 
		break;
	case 12:
		service = "get BDOS version";
		break;
	case 14:
		service = "select native drive, zero-based drive in DL";
		break;
	case 15:
		service = "open file, CP/M FCB at DS:DX";
		break;
	case 16:
		service = "close file, CP/M FCB at DS:DX";
		break;
	case 17: 
		service = "search first, CP/M directory entries in DMA"; 
		break;
	case 18: 
		service = "search next, CP/M directory entries in DMA"; 
		break;
	case 19:
		service = "delete file, CP/M FCB at DS:DX";
		break;
	case 20:
		service = "read sequential 128-byte record";
		break;
	case 21:
		service = "write sequential 128-byte record";
		break;
	case 22:
		service = "create file, CP/M FCB at DS:DX";
		break;
	case 23:
		service = "rename file, new name at FCB+16";
		break;
	case 24:
		service = "native login vector in BX (source HL)";
		break;
	case 25:
		service = "get current drive";
		break;
	case 26:
		service = "set DMA offset from DX";
		break;
	case 27:
		service = "native allocation vector pointer in ES:BX";
		break;
	case 28:
		service = "write protect the current native drive";
		break;
	case 29:
		service = "native read-only drive vector in BX (source HL)";
		break;
	case 30:
		service = "set native CP/M file attributes from FCB name bits";
		break;
	case 31:
		service = "native disk parameter block pointer in ES:BX";
		break;
	case 32:
		service = "get/set native user area, FF queries current user";
		break;
	case 33: 
		service = "random read, 24-bit record at FCB+33"; 
		break;
	case 34: 
		service = "random write, 24-bit record at FCB+33"; 
		break;
	case 35: 
		service = "file size in 128-byte records at FCB+33"; 
		break;
	case 36: 
		service = "set random record from sequential position"; 
		break;
	case 37:
		service = "reset selected native drives, bitmap in DX";
		break;
	case 40: 
		service = "random write with native zero fill"; 
		break;
	default:
		return 0;
	}
	/* The segment is part of these results. Restoring the caller's ES would
	 * destroy the pointer. Do not infer segment overrides for later uses. */
	if (function == 27 || function == 31) {
		instruction(out, "int", value(operand_interrupt, 0xe0), no_operand);
		emit(out, output_comment, service, no_operand, no_operand);
		return bdos_far_result;
	}

	/* CP/M-86 STWFCS/RSTFCB copies the same 33-byte sequential FCB used
	 * by CP/M-80, including CR at offset 32. STWFCR extends this to 36
	 * bytes for random operations, with R0..R2 at offsets 33..35.
	 * File operations return AL=BL
	 * (mapped A=L), use the native DMA, and need no shadow FCB or runtime.
	 * These services have no ES result. Keep the caller's segment. */
	instruction(out, "push", operand(operand_register, "es"), no_operand);
	emit(out, output_comment, service, no_operand, no_operand);
	emit(out, output_line_end, NULL, no_operand, no_operand);
	if (function == 6) {
		/* CP/M-86 reserves FE for status; CP/M 2.2 treats it as output. */
		instruction(out, "cmp", operand(operand_register, "dl"), value(operand_immediate, 254));
		emit(out, output_line_end, NULL, no_operand, no_operand);
		instruction(out, "jne", operand(operand_internal_label, "io6"), value(operand_immediate, site));
		emit(out, output_line_end, NULL, no_operand, no_operand);
		instruction(out, "push", operand(operand_register, "cx"), no_operand);
		emit(out, output_line_end, NULL, no_operand, no_operand);
		instruction(out, "mov", operand(operand_register, "cl"), value(operand_immediate, 2));
		emit(out, output_line_end, NULL, no_operand, no_operand);
		instruction(out, "int", value(operand_interrupt, 0xe0), no_operand);
		emit(out, output_line_end, NULL, no_operand, no_operand);
		instruction(out, "pop", operand(operand_register, "cx"), no_operand);
		emit(out, output_line_end, NULL, no_operand, no_operand);
		instruction(out, "jmp", operand(operand_internal_label, "ioend"), value(operand_immediate, site));
		emit(out, output_line_end, NULL, no_operand, no_operand);
		emit(out, output_internal_definition, "io6", value(operand_immediate, site), no_operand);
	}
	instruction(out, "int", value(operand_interrupt, 0xe0), no_operand);
	emit(out, output_line_end, NULL, no_operand, no_operand);
	if (function == 6) emit(out, output_internal_definition, "ioend", value(operand_immediate, site), no_operand);
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
	.stack_segment = stack_segment,
	.native_bdos = native_bdos,
	.shared_bdos = shared_bdos
};
