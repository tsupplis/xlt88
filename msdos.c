/* Native DOS 1.0 lowering through the shared target builder interface. */
#include "backend.h"
#include "fcbapi.h"
#include "reset.h"
#include "conbuf.h"

static void file_int(const emitter_t *, long);

static void
exit_program(const emitter_t *out)
{
	/* DOS 1.0 loads a COM file with CS pointing to its PSP. */
	instruction(out, "int", value(operand_interrupt, 0x20), no_operand);
	emit(out, output_comment, "terminate DOS COM program", no_operand, no_operand);
}

static int
bdos(const emitter_t *out, long function, unsigned long site)
{
	int returns_byte = 0;
	(void)site;
	const char *service;
	if (function == 13) { reset_disks(out, file_int, 1, site); return 1; }
	if (function == 10) { dos_console_buffer(out, site); return bdos_buffered_result; }

	switch (function) {
	case 1:
		returns_byte = 1;
		service = "DOS console input with echo";
		break;
	case 2:
		service = "DOS console output, character in DL";
		break;
	case 3:
		returns_byte = 1;
		service = "native reader/auxiliary input";
		break;
	case 4:
		service = "native auxiliary output, character in DL";
		break;
	case 5:
		service = "native list output, character in DL";
		break;
	case 9:
		service = "DOS print dollar-terminated string at DS:DX";
		break;
	case 6:
		returns_byte = 1;
		service = "DOS direct console I/O";
		break;
	case 11:
		returns_byte = 1;
		service = "DOS console input status";
		break;
	case 14:
		/* CP/M 2.2 defines no result for select disk. DOS returns its
		 * drive count in AL; it is not a CP/M success/error status. */
		service = "DOS select default drive, zero-based drive in DL";
		break;
	case 19:
		returns_byte = 1;
		service = "DOS delete, CP/M name fields at DS:DX";
		break;
	case 23:
		returns_byte = 1;
		service = "DOS rename, old name at +1 and new name at +17";
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
	if (!fcb_function(function)) return 0;
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
	if (function == 35) {
		/* CP/M size is always in 128-byte records, including unopened FCBs.
		 * Clear R0..R2 first: CP/M leaves a zero count for a missing file,
		 * whereas DOS reports failure without defining those bytes. */
		instruction(out, "mov", operand(operand_register, "si"), operand(operand_register, "dx"));
		emit(out, output_line_end, NULL, no_operand, no_operand);
		instruction(out, "mov", operand(operand_register, "ax"), value(operand_immediate, 128));
		emit(out, output_line_end, NULL, no_operand, no_operand);
		instruction(out, "add", operand(operand_register, "si"), value(operand_immediate, 14));
		emit(out, output_line_end, NULL, no_operand, no_operand);
		instruction(out, "mov", operand(operand_memory, "si"), operand(operand_register, "ax"));
		emit(out, output_line_end, NULL, no_operand, no_operand);
		instruction(out, "xor", operand(operand_register, "ax"), operand(operand_register, "ax"));
		emit(out, output_line_end, NULL, no_operand, no_operand);
		instruction(out, "add", operand(operand_register, "si"), value(operand_immediate, 19));
		emit(out, output_line_end, NULL, no_operand, no_operand);
		instruction(out, "mov", operand(operand_memory, "si"), operand(operand_register, "ax"));
		emit(out, output_line_end, NULL, no_operand, no_operand);
		instruction(out, "add", operand(operand_register, "si"), value(operand_immediate, 2));
		emit(out, output_line_end, NULL, no_operand, no_operand);
		instruction(out, "mov", operand(operand_byte_memory, "si"), operand(operand_register, "al"));
		emit(out, output_line_end, NULL, no_operand, no_operand);
	}
	file_int(out, function);
	if (function == 34) {
		/* DOS combines allocation failures into status 1. CP/M random
		 * write uses 2 for allocation failure (sequential write uses 1).
		 * DOS cannot distinguish the CP/M directory-full status 5. */
		instruction(out, "cmp", operand(operand_register, "al"), value(operand_immediate, 1));
		emit(out, output_line_end, NULL, no_operand, no_operand);
		file_branch(out, "jne", "fileret", site);
		instruction(out, "mov", operand(operand_register, "al"), value(operand_immediate, 2));
		emit(out, output_line_end, NULL, no_operand, no_operand);
	}
	/* Both DOS read operations zero-pad a partial final record but return
	 * 3. CP/M counts that padded record as a successful 128-byte read.
	 * Random positioning stays in the audited native FCB: function 36
	 * derives it from the native sequential fields, never source offsets. */
	if (function == 20 || function == 33) {
		instruction(out, "cmp", operand(operand_register, "al"), value(operand_immediate, 3));
		emit(out, output_line_end, NULL, no_operand, no_operand);
		file_branch(out, "jne", "fileret", site);
		instruction(out, "mov", operand(operand_register, "al"), value(operand_immediate, 0));
		emit(out, output_line_end, NULL, no_operand, no_operand);
	}
	file_label(out, "fileret", site);
	/* Set random record has no byte-result contract. */
	if (function == 36) return 1;
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
