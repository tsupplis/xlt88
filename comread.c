#include <stdlib.h>
#include <string.h>
#include "comread.h"
#include "comhint.h"

enum { com_origin = 0x100, com_limit = 0x10000 };

typedef struct com_image {
	unsigned char bytes[com_limit];
	unsigned char kind[com_limit]; /* 1: code, 2: bytes, 3: pointers, 4: unknown, 5: gap. */
	unsigned char pointer[com_limit]; /* Reviewed or inferred LXI pointer sites. */
	unsigned char width[com_limit]; /* Nonzero only at record boundaries. */
	unsigned char label[com_limit + 1];
	const char *note[com_limit];
	unsigned char hazard[com_limit]; /* Direct writes to code/pointer slots, or CP/M vectors. */
	unsigned end;
} com_image_t;

typedef struct com_instruction {
	char text[48];
	unsigned width;
	int reference;
} com_instruction_t;

/* Keep port hazards separate from discovery notes: one must not hide the
 * other. Check reviewed maps too, since classification is not a native port. */
static void
port_hazards(com_image_t *image)
{
	for (unsigned pc = com_origin; pc < image->end; ++pc) {
		unsigned op = image->bytes[pc], address, size;
		if (image->kind[pc] != 1 || image->width[pc] != 3) continue;
		address = image->bytes[pc+1] | (image->bytes[pc+2] << 8);
		if (op == 0x2a && (address == 1 || address == 6)) image->hazard[pc] |= 4;
		if (op != 0x22 && op != 0x32) continue;
		size = op == 0x22 ? 2 : 1;
		for (unsigned i = 0; i < size; ++i) {
			unsigned at = (address+i) & 65535;
			if (at < com_origin || at >= image->end) continue;
			if (image->kind[at] == 1) image->hazard[pc] |= 1;
			if (image->kind[at] == 3) image->hazard[pc] |= 2;
		}
	}
}

static void
write_hazards(FILE *out, const com_image_t *image, unsigned pc, const char *prefix)
{
	unsigned address = image->bytes[pc+1] | (image->bytes[pc+2] << 8);
	if (image->hazard[pc] & 1)
		fprintf(out, "%sCOM %04x: store at %04x overlaps decoded code; rewriting 8080 bytes requires an explicit native port\n", prefix, pc, address);
	if (image->hazard[pc] & 2)
		fprintf(out, "%sCOM %04x: indirect pointer at %04x is writable; its initial destination is not exhaustive\n", prefix, pc, address);
	if (image->hazard[pc] & 4)
		fprintf(out, "%sCOM %04x: reads CP/M-80 vector at %04x; BIOS addresses and memory limits require a native port\n", prefix, pc, address);
}

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
		if (!strcmp(command, "unknown")) {
			fprintf(diagnostics, "xlt88: COM map line %u: unresolved unknown range; review the ASM80 output and classify it as code, data or word\n", number);
			return 0;
		}
		if (fields == 2 && !strcmp(command, "ptr")) {
			if (first < com_origin || first >= image->end || image->pointer[first]) goto invalid;
			image->pointer[first] = 1;
		} else if (fields == 3 && (!strcmp(command, "code") || !strcmp(command, "data") || !strcmp(command, "word") || !strcmp(command, "gap"))) {
			unsigned kind = !strcmp(command, "code") ? 1 : !strcmp(command, "data") ? 2 : !strcmp(command, "word") ? 3 : 5;
			if (!hex_address(end, &last) || first < com_origin || first >= last || last > image->end || (kind == 3 && (last-first) % 2)) goto invalid;
			for (unsigned address = first; address < last; ++address) {
				if (image->kind[address] || (kind == 5 && image->bytes[address])) goto invalid;
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
		if (address != image->end && (!image->width[address] || image->kind[address] == 5)) return 0;
		snprintf(text, 32, "l%04x", address);
	} else snprintf(text, 32, "%u", address);
	return 1;
}

enum { return_unknown = -65536, return_stack = 16 };

typedef struct return_state {
	int pair[3]; /* BC, DE, HL: offsets from the original return address. */
	int stack[return_stack];
	unsigned depth;
} return_state_t;

/* Prove a callee's return-address adjustment, rather than recognizing an
 * instruction string or a program address. Only affine return-address values
 * are tracked. Every branch must return the same adjustment with balanced
 * stack. Unresolved control flow, memory writes, SP reassignment, recursion
 * and exhausted budgets make the proof unknown, never a guessed summary. */
static int return_adjust(const com_image_t *, unsigned, unsigned, unsigned *);

static int
return_path(const com_image_t *image, unsigned pc, return_state_t state,
    unsigned nesting, unsigned *budget)
{
	while (*budget && pc >= com_origin && pc < image->end) {
		com_instruction_t instruction;
		unsigned op = image->bytes[pc], next, destination = 0, pair = (op >> 4) & 3;
		int conditional_jump = op >= 0xc0 && (op & 7) == 2;
		int conditional_return = op >= 0xc0 && (op & 7) == 0;
		--*budget;
		if (!decode(image->bytes + pc, &instruction) || instruction.width > image->end-pc)
			return return_unknown;
		next = pc + instruction.width;
		if (instruction.width == 3) destination = image->bytes[pc+1] | (image->bytes[pc+2] << 8);
		if (op == 0xc9 || conditional_return) {
			int result = state.depth == 1 ? state.stack[0] : return_unknown;
			if (result < 0 || result > 64) return return_unknown;
			if (conditional_return && return_path(image, next, state, nesting, budget) != result)
				return return_unknown;
			return result;
		}
		if (op == 0xc3 || conditional_jump) {
			if (conditional_jump) {
				int taken = return_path(image, destination, state, nesting, budget);
				if (taken == return_unknown || return_path(image, next, state, nesting, budget) != taken)
					return return_unknown;
				return taken;
			}
			pc = destination;
			continue;
		}
		if (op == 0xcd || (op >= 0xc0 && (op & 7) == 4)) {
			int advance = return_adjust(image, destination, nesting+1, budget);
			if (advance < 0 || (op != 0xcd && advance)) return return_unknown;
			next += (unsigned)advance;
			for (unsigned i = 0; i < 3; ++i) state.pair[i] = return_unknown;
		} else if (op == 0xe3) {
			int old;
			if (!state.depth) return return_unknown;
			old = state.stack[state.depth-1];
			state.stack[state.depth-1] = state.pair[2];
			state.pair[2] = old;
		} else if (op >= 0xc0 && (op & 15) == 5) {
			if (state.depth == return_stack) return return_unknown;
			state.stack[state.depth++] = pair < 3 ? state.pair[pair] : return_unknown;
		} else if (op >= 0xc0 && (op & 15) == 1) {
			if (!state.depth) return return_unknown;
			--state.depth;
			if (pair < 3) state.pair[pair] = state.stack[state.depth];
		} else if (op == 0xeb) {
			int old = state.pair[1];
			state.pair[1] = state.pair[2]; state.pair[2] = old;
		} else if (op == 0xe9 || op == 0xf9 || op == 0x76 ||
		    (op >= 0xc0 && (op & 7) == 7)) return return_unknown;
		else if (op < 0x40 && ((op & 15) == 1 || (op & 15) == 3 || (op & 15) == 11)) {
			if (pair == 3) return return_unknown;
			if ((op & 15) == 1) state.pair[pair] = return_unknown;
			else if (state.pair[pair] != return_unknown) state.pair[pair] += (op & 15) == 3 ? 1 : -1;
		} else if (op == 0x2a || (op < 0x40 && (op & 15) == 9)) state.pair[2] = return_unknown;
		else if (op == 0x02 || op == 0x12 || op == 0x22 || op == 0x32)
			return return_unknown; /* Unproved store could alias the stack. */
		else if ((op >= 0x40 && op < 0x80) ||
		    (op < 0x40 && (op & 7) >= 4 && (op & 7) <= 6)) {
			unsigned reg = (op >> 3) & 7;
			if (reg == 6) return return_unknown;
			if (reg < 6) state.pair[reg/2] = return_unknown;
		}
		pc = next;
	}
	return return_unknown;
}

static int
return_adjust(const com_image_t *image, unsigned pc, unsigned nesting, unsigned *budget)
{
	return_state_t state = { .pair = {return_unknown, return_unknown, return_unknown},
	    .stack = {0}, .depth = 1 };
	if (nesting > 8) return return_unknown;
	return return_path(image, pc, state, nesting, budget);
}

/* Some runtime helpers pop the return address, read an inline target, push
 * the adjusted continuation, and jump to that target. Evaluate a bounded
 * straight-line helper using the same literal/stack domain as discovery.
 * This is a candidate summary, requiring the same no-stack-alias review. */
static int
inline_target(const com_image_t *image, unsigned pc, unsigned resume, unsigned *advance)
{
	com_hint_t state;
	unsigned last = image->end - resume < 64 ? image->end : resume + 64;
	com_hint_init(&state);
	state.stack[state.depth++] = (int)resume;
	for (unsigned steps = 0; steps < 64 && pc >= com_origin && pc < image->end; ++steps) {
		com_instruction_t instruction;
		unsigned op = image->bytes[pc];
		int destination;
		if (!decode(image->bytes+pc, &instruction) || instruction.width > image->end-pc) break;
		if (op == 0xe9 || op == 0xc9) {
			destination = com_hint_step(&state, image->bytes+pc);
			if (state.depth == 1 && state.stack[0] > (int)resume && state.stack[0] <= (int)last &&
			    (op == 0xc9 || (destination >= com_origin && (unsigned)destination < image->end))) {
				*advance = (unsigned)state.stack[0] - resume;
				return op == 0xc9 ? -2 : destination;
			}
			break;
		}
		if (op == 0xc3 || op == 0x76 ||
		    (op >= 0xc0 && ((op & 7) == 0 || (op & 7) == 2 || (op & 7) == 4 || (op & 7) == 7))) break;
		if (!com_hint_read(&state, image->bytes+pc, image->bytes, resume, last))
			com_hint_step(&state, image->bytes+pc);
		pc += instruction.width;
	}
	return -1;
}

/* A bounded work list follows direct transfers and literal/initial-pointer
 * candidates. Bytes not reached remain unknown: they may contain indirect
 * entries or tables. Call continuations and inferred data need review too;
 * balanced calls and unchanged initial pointers are not whole-program proofs. */
static int
discover(com_image_t *image, FILE *map, FILE *diagnostics)
{
	typedef struct com_entry { unsigned pc; com_hint_t hint; } com_entry_t;
	com_entry_t *queue = malloc(com_limit * sizeof(*queue));
	unsigned char *queued = calloc(com_limit, 1);
	unsigned head = 0, tail = 0, code = 0, data = 0;
	if (!queue || !queued) { free(queue); free(queued); return 0; }
	queue[tail].pc = com_origin;
	com_hint_init(&queue[tail++].hint);
	queued[com_origin] = 1;
	while (head < tail) {
		unsigned pc = queue[head].pc;
		com_hint_t hint = queue[head++].hint;
		while (pc < image->end) {
			com_instruction_t instruction;
			unsigned op = image->bytes[pc], width, i, destination = 0;
			int transfer = op == 0xc3 || op == 0xcd ||
			    (op >= 0xc0 && ((op & 7) == 2 || (op & 7) == 4));
			if (image->kind[pc]) {
				if (!image->width[pc]) image->note[pc] = "control flow enters an instruction operand; resolve overlapping code";
				else if (image->kind[pc] == 2 || image->kind[pc] == 3) image->note[pc] = "control flow conflicts with inferred data; review the classification";
				break;
			}
			if (!decode(image->bytes + pc, &instruction) || instruction.width > image->end - pc) {
				image->note[pc] = "reachable invalid or truncated 8080 instruction; check CPU dialect and code boundary";
				break;
			}
			width = instruction.width;
			for (i = 0; i < width && !image->kind[pc+i]; ++i) {}
			if (i != width) {
				image->note[pc] = "instruction overlaps another discovered path; resolve code boundary";
				break;
			}
			image->width[pc] = (unsigned char)width;
			for (i = 0; i < width; ++i) image->kind[pc+i] = 1;
			code += width;
			if (width == 3) destination = image->bytes[pc+1] | (image->bytes[pc+2] << 8);
			if (transfer && op != 0xc3 && destination >= com_origin && destination < image->end && !queued[destination]) {
				queue[tail].pc = destination;
				if (op == 0xcd || (op & 7) == 4) com_hint_init(&queue[tail].hint);
				else queue[tail].hint = hint;
				++tail;
				queued[destination] = 1;
			}
			if (transfer && destination != 0 && destination != 5 &&
			    (destination < com_origin || destination >= image->end))
				image->note[pc] = "transfer outside image; supply missing code or port the external entry";
			if ((op & 0xcf) == 1 && destination >= com_origin)
				image->note[pc] = "LXI may load an address; add ptr here if it is a pointer, otherwise retain the numeric constant";
			if (op == 0xe9) image->note[pc] = "PCHL destination unknown; classify indirect entries and pointer tables";
			if (op == 0xe3) image->note[pc] = "XTHL may access inline return-address data; review call fall-through classification";
			if (op >= 0xcf && (op & 7) == 7) image->note[pc] = "RST monitor service requires a manual port";
			int candidate = com_hint_step(&hint, image->bytes + pc);
			/* LHLD slot / PCHL consumes a word as an indirect entry address.
			 * Recover its initial value only: runtime writes can change it.
			 * Never reinterpret an already discovered instruction as a table. */
			if (op == 0x2a && pc+width < image->end && image->bytes[pc+width] == 0xe9 &&
			    destination >= com_origin && destination+1 < image->end &&
			    !image->kind[destination] && !image->kind[destination+1] &&
			    (destination+1 < pc || destination > pc+width)) {
				unsigned entry = image->bytes[destination] | (image->bytes[destination+1] << 8);
				com_instruction_t target;
				if (entry >= com_origin && entry < image->end &&
				    entry != destination && entry != destination+1 &&
				    (!image->kind[entry] || (image->kind[entry] == 1 && image->width[entry])) &&
				    decode(image->bytes+entry, &target) && target.width <= image->end-entry) {
					image->kind[destination] = image->kind[destination+1] = 3;
					image->width[destination] = 2;
					image->label[destination] = 1;
					hint.reg[4] = entry >> 8;
					hint.reg[5] = entry & 255;
					data += 2;
					image->note[destination] = "initial indirect pointer; review runtime writes and additional destinations";
				}
			}
			if (candidate >= com_origin && (unsigned)candidate < image->end) {
				image->label[candidate] = 1;
				if (!queued[candidate]) {
					queue[tail].pc = (unsigned)candidate;
					queue[tail].hint = hint;
					if (op != 0xe9 && queue[tail].hint.depth) --queue[tail].hint.depth;
					++tail;
					queued[candidate] = 1;
				}
				image->note[pc] = "candidate indirect entry recovered from literals/stack; verify balanced calls and absence of stack aliasing";
			}
			if (op == 0xcd) {
				unsigned budget = 256;
				int advance = return_adjust(image, destination, 0, &budget);
				unsigned skipped = 0;
				int indirect = inline_target(image, destination, pc+width, &skipped);
				int inferred = indirect != -1 && advance < 0;
				if (inferred) advance = (int)skipped;
				if (advance > 0 && (unsigned)advance <= image->end-pc-width) {
					unsigned end = pc + width + (unsigned)advance;
					for (i = pc+width; i < end && !image->kind[i]; ++i) {}
					if (i == end) {
						for (i = pc+width; i < end; ++i) { image->kind[i] = 2; image->width[i] = 1; }
						data += (unsigned)advance;
						image->note[pc] = "callee advances its return address; inline bytes follow and require a native port";
						if (inferred) image->note[pc] = "candidate inline arguments inferred assuming balanced calls; review continuation and native port";
						if (indirect >= 0) {
							image->note[pc] = "callee reads an inline jump target; candidate entry and continuation require a native port";
							image->label[indirect] = 1;
							if (!queued[indirect]) {
								queue[tail].pc = (unsigned)indirect;
								com_hint_init(&queue[tail++].hint);
								queued[indirect] = 1;
							}
						}
						width += (unsigned)advance;
					}
				}
			}
			if (op == 0xc3 && destination >= com_origin && destination < image->end) {
				pc = destination;
				continue;
			}
			if (op == 0xc3 || op == 0xc9 || op == 0xe9 || op == 0x76 ||
			    (op >= 0xc7 && (op & 7) == 7) || (op == 0xcd && !destination)) break;
			pc += width;
			if (pc == image->end) image->note[pc-width] = "execution falls beyond image; review missing code or termination";
		}
		/* An initialized dispatch slot may be replaced before it is used.
		 * Follow explicit LXI H / SHLD writes to proven indirect words, not
		 * arbitrary constants that merely resemble code addresses. Repeat
		 * after newly reached paths, with each entry queued at most once. */
		if (head == tail) {
			for (unsigned at = com_origin; at+5 < image->end; ++at) {
				unsigned entry, slot;
				if (image->kind[at] != 1 || image->width[at] != 3 || image->bytes[at] != 0x21 ||
				    image->kind[at+3] != 1 || image->width[at+3] != 3 || image->bytes[at+3] != 0x22) continue;
				entry = image->bytes[at+1] | (image->bytes[at+2] << 8);
				slot = image->bytes[at+4] | (image->bytes[at+5] << 8);
				if (image->kind[slot] != 3 || image->width[slot] != 2 ||
				    entry < com_origin || entry >= image->end ||
				    (image->kind[entry] && (image->kind[entry] != 1 || !image->width[entry]))) continue;
				image->pointer[at] = 1;
				image->label[entry] = 1;
				image->note[at] = "address stored into indirect pointer; candidate destination recovered, review other writes";
				if (!queued[entry]) {
					queue[tail].pc = entry;
					com_hint_init(&queue[tail++].hint);
					queued[entry] = 1;
				}
			}
		}
	}
	free(queue);
	free(queued);
	/* Add candidate relocations only for values actually used as addresses.
	 * Unknown out-of-image storage remains numeric until layout is supplied. */
	for (unsigned pc = com_origin; pc+2 < image->end; ++pc) {
		unsigned address;
		if (image->kind[pc] != 1 || image->width[pc] != 3 || (image->bytes[pc] & 0xcf) != 1) continue;
		address = image->bytes[pc+1] | (image->bytes[pc+2] << 8);
		if (address < com_origin || address > image->end ||
		    (address < image->end && image->kind[address] == 1 && !image->width[address])) continue;
		if (com_ptr_used(image->bytes, image->kind, image->width, image->end, pc)) {
			image->pointer[pc] = 1;
			image->label[address] = 1;
			image->note[pc] = "LXI used as address; candidate ptr annotation added, review other uses and aliasing";
		}
	}
	port_hazards(image);
	fputs("# Draft discovery, not a verified layout. Addresses are hexadecimal.\n"
	    "# Review code, replace unknown ranges with code/data/word, add ptr at pointer LXI sites.\n"
	    "# Indirect entries are candidates; runtime writes and additional targets need review.\n", map);
	for (unsigned pc = com_origin; pc < image->end;) {
		unsigned end = pc + 1;
		while (end < image->end && image->kind[end] == image->kind[pc]) ++end;
		fprintf(map, "%s %04x %04x\n", image->kind[pc] == 1 ? "code" : image->kind[pc] == 2 ? "data" : image->kind[pc] == 3 ? "word" : "unknown", pc, end);
		pc = end;
	}
	for (unsigned pc = com_origin; pc < image->end; ++pc) {
		if (image->pointer[pc]) fprintf(map, "ptr %04x\n", pc);
		if (image->note[pc]) fprintf(map, "# %04x: %s\n", pc, image->note[pc]);
		if (image->hazard[pc]) write_hazards(map, image, pc, "# ");
		if (!image->kind[pc]) {
			image->width[pc] = 1;
			/* Kind 4 is unknown; DB is only its lossless review representation. */
			image->kind[pc] = 4;
		}
	}
	fprintf(diagnostics, "xlt88: COM analysis: %u candidate code bytes, %u candidate data bytes, %u unknown bytes; review required\n",
	    code, data, image->end - com_origin - code - data);
	return !ferror(map);
}

/* Text is a lossless presentation choice, never evidence of data ownership.
 * Require four printable ASCII bytes; controls and quotes remain numeric so
 * the recovered source is portable across the supported assembler dialects. */
static void
write_bytes(FILE *output, const unsigned char *bytes, unsigned size)
{
	for (unsigned i = 0; i < size;) {
		unsigned end = i;
		while (end < size && bytes[end] >= 32 && bytes[end] <= 126 && bytes[end] != '\'') ++end;
		if (i) fputc(',', output);
		if (end-i >= 4) {
			fputc('\'', output);
			while (i < end) fputc(bytes[i++], output);
			fputc('\'', output);
		} else fprintf(output, "%u", bytes[i++]);
	}
}

static int
read_image(FILE *binary, FILE *map, FILE *draft, source_program_t *source, FILE *diagnostics)
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
	if (draft) {
		if (!discover(image, draft, diagnostics)) goto done;
	} else if (!read_map(map, image, diagnostics)) goto done;
	/* Establish every boundary before resolving direct addresses. Never decode
	 * bytes marked as data, and never let an instruction cross an annotation. */
	for (; !draft && pc < image->end;) {
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
	if (!draft && image->kind[com_origin] != 1) { pc = com_origin; goto invalid; }
	port_hazards(image);
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
			if (!address_text(image, address, text)) {
				if (!draft) goto invalid;
				image->note[pc] = "address points inside an instruction; numeric operand retained for review";
				continue;
			}
			if (address >= com_origin && address <= image->end) image->label[address] = 1;
		}
	}
	flat = tmpfile();
	if (!flat) goto done;
	fputs(" org 100h\n", flat);
	if (draft) fputs(" ; XLT88 TODO: discovery draft only; review map, pointers and code/data boundaries before translation\n", flat);
	for (pc = com_origin; pc < image->end;) {
		char operand[32];
		unsigned width = image->width[pc];
		/* A reviewed zero-filled gap expresses an original fixed origin.
		 * Do not carry its padding into expanded native code. */
		if (image->kind[pc] == 5) {
			while (pc < image->end && image->kind[pc] == 5) ++pc;
			fprintf(flat, " org %u\n", pc);
			continue;
		}
		int bytes = image->kind[pc] == 2 || image->kind[pc] == 4;
		/* Keep byte runs compact without hiding any referenced address, note
		 * or classification boundary. Per-byte labels burden all downstream
		 * analyses and add no information for unreferenced literal storage. */
		if (bytes) {
			while (width < 16 && pc + width < image->end &&
			    image->kind[pc+width] == image->kind[pc] &&
			    !image->label[pc+width] && !image->note[pc+width]) ++width;
			/* Continue a printable run rather than cutting words every sixteen
			 * bytes. Bound line length for the original CP/M assemblers. */
			while (width >= 16 && width < 64 && pc+width < image->end &&
			    image->bytes[pc+width-1] >= 32 && image->bytes[pc+width-1] <= 126 &&
			    image->bytes[pc+width] >= 32 && image->bytes[pc+width] <= 126 &&
			    image->kind[pc+width] == image->kind[pc] &&
			    !image->label[pc+width] && !image->note[pc+width]) ++width;
		}
		/* Include notes on operand bytes too when a competing path lands there. */
		for (unsigned i = 0; i < width; ++i)
			if (image->note[pc+i]) fprintf(flat, " ; XLT88 TODO: COM %04x: %s\n", pc+i, image->note[pc+i]);
		if (image->hazard[pc]) write_hazards(flat, image, pc, " ; XLT88 TODO: ");
		if (image->kind[pc] == 4 && (pc == com_origin || image->kind[pc-1] != 4))
			fprintf(flat, " ; XLT88 TODO: COM %04x: unknown region follows; DB preserves bytes without classifying them as data\n", pc);
		if (image->label[pc] || (image->kind[pc] != 1 &&
		    (pc == com_origin || image->kind[pc-1] != image->kind[pc] ||
		    image->kind[pc] == 3 || image->note[pc]))) fprintf(flat, "l%04x: ", pc);
		else fputc(' ', flat);
		if (bytes) {
			fputs("db ", flat);
			write_bytes(flat, image->bytes+pc, width);
		}
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
					if (!address_text(image, value, operand)) {
						if (!draft) goto invalid;
						snprintf(operand, sizeof(operand), "%u", value);
					}
				} else snprintf(operand, sizeof(operand), "%u", value);
				fprintf(flat, "%s%s", instruction.text[strlen(instruction.text)-1] == ',' ? "" : " ", operand);
			}
		}
		fputc('\n', flat);
		pc += width;
	}
	if (image->label[image->end]) fprintf(flat, "l%04x:\n", image->end);
	fputs(" end\n", flat);
	if (ferror(flat)) goto done;
	rewind(flat);
	result = source_read(&asm80_dialect, flat, NULL, source);
	goto done;
invalid:
	fprintf(diagnostics, "xlt88: COM address %04x: unclassified byte, invalid opcode, split instruction or invalid pointer boundary\n", pc);
done:
	if (flat) fclose(flat);
	free(image);
	return result;
}

int
com_read(FILE *binary, FILE *map, source_program_t *source, FILE *diagnostics)
{
	return read_image(binary, map, NULL, source, diagnostics);
}

int
com_analyze(FILE *binary, FILE *draft, source_program_t *source, FILE *diagnostics)
{
	return read_image(binary, NULL, draft, source, diagnostics);
}
