#include <stdlib.h>
#include <string.h>
#include "comread.h"

enum { com_origin = 0x100, com_limit = 0x10000 };

typedef struct com_image {
	unsigned char bytes[com_limit];
	unsigned char kind[com_limit]; /* 1: code, 2: bytes, 3: pointer words. */
	unsigned char pointer[com_limit]; /* LXI sites explicitly holding pointers. */
	unsigned char width[com_limit]; /* Nonzero only at record boundaries. */
	unsigned char label[com_limit + 1];
	unsigned end;
} com_image_t;

typedef struct com_instruction {
	char text[48];
	unsigned width;
	int reference;
} com_instruction_t;

/* Decode only documented 8080 opcodes; undocumented NOP/JMP/RET/CALL aliases
 * are rejected rather than silently assigning a processor-specific meaning. */
static int
decode(const unsigned char *bytes, com_instruction_t *out)
{
	static const char *const reg[] = {"b", "c", "d", "e", "h", "l", "m", "a"};
	static const char *const pair[] = {"b", "d", "h", "sp"};
	static const char *const stack[] = {"b", "d", "h", "psw"};
	static const char *const alu[] = {"add", "adc", "sub", "sbb", "ana", "xra", "ora", "cmp"};
	static const char *const immediate[] = {"adi", "aci", "sui", "sbi", "ani", "xri", "ori", "cpi"};
	static const char *const condition[] = {"nz", "z", "nc", "c", "po", "pe", "p", "m"};
	unsigned op = bytes[0], row = (op >> 3) & 7;
	const char *name = NULL;
	memset(out, 0, sizeof(*out));
	out->width = 1;
	if (op >= 0x40 && op < 0x80) {
		if (op == 0x76) strcpy(out->text, "hlt");
		else snprintf(out->text, sizeof(out->text), "mov %s,%s", reg[row], reg[op & 7]);
	} else if (op >= 0x80 && op < 0xc0) {
		snprintf(out->text, sizeof(out->text), "%s %s", alu[row], reg[op & 7]);
	} else if (op < 0x40 && (op & 7) >= 4 && (op & 7) <= 6) {
		unsigned form = op & 7;
		snprintf(out->text, sizeof(out->text), "%s %s", form == 4 ? "inr" : form == 5 ? "dcr" : "mvi", reg[row]);
		if (form == 6) { strcat(out->text, ","); out->width = 2; }
	} else if (op < 0x40 && ((op & 15) == 1 || (op & 15) == 3 || (op & 15) == 9 || (op & 15) == 11)) {
		unsigned form = op & 15;
		snprintf(out->text, sizeof(out->text), "%s %s", form == 1 ? "lxi" : form == 3 ? "inx" : form == 9 ? "dad" : "dcx", pair[op >> 4]);
		if (form == 1) { strcat(out->text, ","); out->width = 3; }
	} else if (op >= 0xc0 && ((op & 7) == 0 || (op & 7) == 2 || (op & 7) == 4)) {
		unsigned form = op & 7;
		snprintf(out->text, sizeof(out->text), "%c%s", form == 0 ? 'r' : form == 2 ? 'j' : 'c', condition[row]);
		if (form != 0) { out->width = 3; out->reference = 1; }
	} else if (op >= 0xc0 && ((op & 15) == 1 || (op & 15) == 5)) {
		snprintf(out->text, sizeof(out->text), "%s %s", (op & 15) == 1 ? "pop" : "push", stack[(op >> 4) & 3]);
	} else if (op >= 0xc0 && (op & 7) == 6) {
		strcpy(out->text, immediate[row]); out->width = 2;
	} else if (op >= 0xc0 && (op & 7) == 7) {
		snprintf(out->text, sizeof(out->text), "rst %u", row);
	} else {
		switch (op) {
		case 0x00: name = "nop"; break;
		case 0x02: name = "stax b"; break;
		case 0x12: name = "stax d"; break;
		case 0x0a: name = "ldax b"; break;
		case 0x1a: name = "ldax d"; break;
		case 0x07: name = "rlc"; break;
		case 0x0f: name = "rrc"; break;
		case 0x17: name = "ral"; break;
		case 0x1f: name = "rar"; break;
		case 0x27: name = "daa"; break;
		case 0x2f: name = "cma"; break;
		case 0x37: name = "stc"; break;
		case 0x3f: name = "cmc"; break;
		case 0x22: name = "shld"; out->width = 3; break;
		case 0x2a: name = "lhld"; out->width = 3; break;
		case 0x32: name = "sta"; out->width = 3; break;
		case 0x3a: name = "lda"; out->width = 3; break;
		case 0xc3: name = "jmp"; out->width = 3; break;
		case 0xcd: name = "call"; out->width = 3; break;
		case 0xc9: name = "ret"; break;
		case 0xd3: name = "out"; out->width = 2; break;
		case 0xdb: name = "in"; out->width = 2; break;
		case 0xe3: name = "xthl"; break;
		case 0xe9: name = "pchl"; break;
		case 0xeb: name = "xchg"; break;
		case 0xf3: name = "di"; break;
		case 0xf9: name = "sphl"; break;
		case 0xfb: name = "ei"; break;
		default: return 0;
		}
		strcpy(out->text, name);
		out->reference = out->width == 3;
	}
	return 1;
}

static int
hex_address(const char *text, unsigned *value)
{
	unsigned number = 0;
	size_t length = strlen(text);
	if (!length || length > 5) return 0;
	for (size_t i = 0; i < length; ++i) {
		unsigned digit;
		if (text[i] >= '0' && text[i] <= '9') digit = (unsigned)(text[i] - '0');
		else if (text[i] >= 'a' && text[i] <= 'f') digit = (unsigned)(text[i] - 'a' + 10);
		else if (text[i] >= 'A' && text[i] <= 'F') digit = (unsigned)(text[i] - 'A' + 10);
		else return 0;
		number = number * 16 + digit;
	}
	if (number > com_limit) return 0;
	*value = number;
	return 1;
}

static int
read_map(FILE *map, com_image_t *image, FILE *diagnostics)
{
	char line[256], command[16], start[32], end[32], extra;
	unsigned first, last, number = 0;
	for (;;) {
		char *comment;
		size_t length = 0;
		int fields, ch;
		++number;
		while ((ch = fgetc(map)) != EOF && ch != '\n') {
			if (!ch || length == sizeof(line)-1) goto invalid;
			line[length++] = (char)ch;
		}
		if (ch == EOF && !length) break;
		line[length] = 0;
		comment = strchr(line, '#');
		if (comment) *comment = 0;
		fields = sscanf(line, "%15s %31s %31s %c", command, start, end, &extra);
		if (fields == EOF) continue;
		if (fields < 2 || !hex_address(start, &first)) goto invalid;
		if (fields == 2 && !strcmp(command, "ptr")) {
			if (first < com_origin || first >= image->end || image->pointer[first]) goto invalid;
			image->pointer[first] = 1;
		} else if (fields == 3 && (!strcmp(command, "code") || !strcmp(command, "data") || !strcmp(command, "word"))) {
			unsigned kind = !strcmp(command, "code") ? 1 : !strcmp(command, "data") ? 2 : 3;
			if (!hex_address(end, &last) || first < com_origin || first >= last || last > image->end || (kind == 3 && (last-first) % 2)) goto invalid;
			for (unsigned address = first; address < last; ++address) {
				if (image->kind[address]) goto invalid;
				image->kind[address] = kind;
			}
		} else goto invalid;
	}
	return !ferror(map);
invalid:
	fprintf(diagnostics, "xlt88: COM map line %u: invalid, overlapping or out-of-range annotation\n", number);
	return 0;
}

static int
address_text(const com_image_t *image, unsigned address, char text[32])
{
	if (address >= com_origin && address <= image->end) {
		if (address != image->end && !image->width[address]) return 0;
		snprintf(text, 32, "l%04x", address);
	} else snprintf(text, 32, "%u", address);
	return 1;
}

int
com_read(FILE *binary, FILE *map, source_program_t *source, FILE *diagnostics)
{
	com_image_t *image = calloc(1, sizeof(*image));
	FILE *flat = NULL;
	int result = 0;
	unsigned pc = com_origin;
	size_t size;
	if (!image) return 0;
	size = fread(image->bytes + com_origin, 1, com_limit - com_origin, binary);
	image->end = com_origin + (unsigned)size;
	if (!size || ferror(binary) || fgetc(binary) != EOF || ferror(binary)) {
		fputs("xlt88: COM image must contain 1..65280 bytes\n", diagnostics);
		goto done;
	}
	if (!read_map(map, image, diagnostics)) goto done;
	/* Establish every boundary before resolving direct addresses. Never decode
 * bytes marked as data, and never let an instruction cross an annotation. */
	for (; pc < image->end;) {
		com_instruction_t instruction;
		unsigned width = image->kind[pc] == 3 ? 2 : 1;
		if (!image->kind[pc]) goto invalid;
		if (image->kind[pc] == 1) {
			if (!decode(image->bytes + pc, &instruction)) goto invalid;
			width = instruction.width;
		}
		if (width > image->end - pc) goto invalid;
		for (unsigned i = 0; i < width; ++i) {
			if (image->kind[pc+i] != image->kind[pc] || (i && image->pointer[pc+i])) goto invalid;
		}
		if (image->pointer[pc] && (image->kind[pc] != 1 || (image->bytes[pc] & 0xcf) != 1)) goto invalid;
		image->width[pc] = (unsigned char)width;
		pc += width;
	}
	if (image->kind[com_origin] != 1) { pc = com_origin; goto invalid; }
	image->label[com_origin] = 1;
	/* Label referenced code boundaries only. Inventing a label at every
 * instruction would introduce false entry points for existing analyses. */
	for (pc = com_origin; pc < image->end; pc += image->width[pc]) {
		int reference = image->kind[pc] == 3 || image->pointer[pc];
		unsigned operand = pc;
		if (image->kind[pc] == 1) {
			com_instruction_t instruction;
			decode(image->bytes + pc, &instruction);
			reference |= instruction.reference;
			++operand;
		}
		if (reference) {
			char text[32];
			unsigned address = image->bytes[operand] | (image->bytes[operand+1] << 8);
			if (!address_text(image, address, text)) goto invalid;
			if (address >= com_origin && address <= image->end) image->label[address] = 1;
		}
	}
	flat = tmpfile();
	if (!flat) goto done;
	fputs(" org 100h\n", flat);
	for (pc = com_origin; pc < image->end; pc += image->width[pc]) {
		char operand[32];
		unsigned width = image->width[pc];
		if (image->label[pc] || image->kind[pc] != 1) fprintf(flat, "l%04x: ", pc);
		else fputc(' ', flat);
		if (image->kind[pc] == 2) fprintf(flat, "db %u", image->bytes[pc]);
		else if (image->kind[pc] == 3) {
			unsigned address = image->bytes[pc] | (image->bytes[pc+1] << 8);
			if (!address_text(image, address, operand)) goto invalid;
			fprintf(flat, "dw %s", operand);
		} else {
			com_instruction_t instruction;
			decode(image->bytes + pc, &instruction);
			fputs(instruction.text, flat);
			if (width > 1) {
				unsigned value = image->bytes[pc+1];
				if (width == 3) value |= image->bytes[pc+2] << 8;
				if (instruction.reference || image->pointer[pc]) {
					if (!address_text(image, value, operand)) goto invalid;
				} else snprintf(operand, sizeof(operand), "%u", value);
				fprintf(flat, "%s%s", instruction.text[strlen(instruction.text)-1] == ',' ? "" : " ", operand);
			}
		}
		fputc('\n', flat);
	}
	if (image->label[image->end]) fprintf(flat, "l%04x:\n", image->end);
	fputs(" end\n", flat);
	if (ferror(flat)) goto done;
	rewind(flat);
	result = source_read(&intel_dialect, flat, NULL, source);
	goto done;
invalid:
	fprintf(diagnostics, "xlt88: COM address %04x: unclassified byte, invalid opcode, split instruction or invalid pointer boundary\n", pc);
done:
	if (flat) fclose(flat);
	free(image);
	return result;
}
