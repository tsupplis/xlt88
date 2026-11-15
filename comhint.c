#include "comhint.h"

static int
pair_value(const com_hint_t *state, unsigned pair)
{
	unsigned reg = pair * 2;
	return pair < 3 && state->reg[reg] >= 0 && state->reg[reg+1] >= 0 ?
	    (state->reg[reg] << 8) | state->reg[reg+1] : -1;
}

static void
set_pair(com_hint_t *state, unsigned pair, int value)
{
	if (pair < 3) {
		state->reg[pair*2] = value < 0 ? -1 : (value >> 8) & 255;
		state->reg[pair*2+1] = value < 0 ? -1 : value & 255;
	}
}

void
com_hint_init(com_hint_t *state)
{
	for (unsigned i = 0; i < 8; ++i) state->reg[i] = -1;
	state->depth = 0;
}

/* Read literals only from the bounded inline argument window supplied by the
 * caller. This does not treat arbitrary program memory as immutable storage. */
int
com_hint_read(com_hint_t *state, const unsigned char *bytes,
    const unsigned char *image, unsigned first, unsigned last)
{
	unsigned op = bytes[0];
	int address = pair_value(state, 2);
	if (op >= 0x40 && op < 0x80 && op != 0x76 && (op & 7) == 6 &&
	    address >= (int)first && address < (int)last) {
		state->reg[(op >> 3) & 7] = image[address];
		return 1;
	}
	return 0;
}

/* Called only for a validated, complete 8080 instruction. Keep literal values
 * through straight-line code and tail jumps. Indirect memory is never read as
 * a constant: writes, relocation and runtime OS state may change it. */
int
com_hint_step(com_hint_t *state, const unsigned char *bytes)
{
	unsigned op = bytes[0], reg = (op >> 3) & 7, pair = (op >> 4) & 3;
	int value;
	if (op == 0xc9 || (op >= 0xc0 && (op & 7) == 0))
		return state->depth ? state->stack[state->depth-1] : -1;
	if (op == 0xe9) return pair_value(state, 2);
	if (op == 0xcd || (op >= 0xc0 && (op & 7) == 4)) {
		for (unsigned i = 0; i < 8; ++i) state->reg[i] = -1;
	} else if (op == 0xf9 || op == 0x31 || op == 0x33 || op == 0x3b) state->depth = 0;
	else if (op < 0x40 && (op & 15) == 1) set_pair(state, pair, bytes[1] | (bytes[2] << 8));
	else if (op < 0x40 && ((op & 15) == 3 || (op & 15) == 11)) {
		value = pair_value(state, pair);
		set_pair(state, pair, value < 0 ? -1 : (value + ((op & 15) == 3 ? 1 : -1)) & 65535);
	} else if (op < 0x40 && (op & 15) == 9) {
		value = pair_value(state, pair);
		int hl = pair_value(state, 2);
		set_pair(state, 2, value < 0 || hl < 0 ? -1 : (value + hl) & 65535);
	} else if (op >= 0x40 && op < 0x80 && op != 0x76) {
		if (reg != 6) state->reg[reg] = (op & 7) == 6 ? -1 : state->reg[op & 7];
	} else if (op < 0x40 && (op & 7) == 6) {
		if (reg != 6) state->reg[reg] = bytes[1];
	} else if (op < 0x40 && ((op & 7) == 4 || (op & 7) == 5)) {
		if (reg != 6 && state->reg[reg] >= 0)
			state->reg[reg] = (state->reg[reg] + ((op & 7) == 4 ? 1 : -1)) & 255;
	} else if (op >= 0xc0 && (op & 15) == 5) {
		if (state->depth == 16) state->depth = 0;
		else state->stack[state->depth++] = pair_value(state, pair);
	} else if (op >= 0xc0 && (op & 15) == 1) {
		value = state->depth ? state->stack[--state->depth] : -1;
		if (pair == 3) state->reg[7] = -1;
		else set_pair(state, pair, value);
	} else if (op == 0xeb) {
		value = pair_value(state, 1);
		set_pair(state, 1, pair_value(state, 2)); set_pair(state, 2, value);
	} else if (op == 0xe3) {
		value = state->depth ? state->stack[state->depth-1] : -1;
		if (state->depth) state->stack[state->depth-1] = pair_value(state, 2);
		set_pair(state, 2, value);
	} else if (op == 0x2a) set_pair(state, 2, -1);
	else if ((op >= 0x80 && op < 0xc0) || (op >= 0xc0 && (op & 7) == 6) ||
	    op == 0x0a || op == 0x1a || op == 0x3a || op == 0xdb ||
	    op == 7 || op == 15 || op == 23 || op == 31 || op == 0x27 || op == 0x2f)
		state->reg[7] = -1;
	return -1;
}

typedef struct pointer_use {
	unsigned char reg[8]; /* 1: original high byte, 2: original low byte. */
	unsigned char stack[16][2];
	unsigned depth;
	int service;
} pointer_use_t;

static int
pointer_pair(const pointer_use_t *state, unsigned pair)
{
	return pair < 3 && state->reg[pair*2] == 1 && state->reg[pair*2+1] == 2;
}

/* Search for an address use, not a numeric resemblance to a label. Copies and
 * whole-pair increments retain provenance; byte arithmetic and overwrites do
 * not. Calls are examined with an unknown return frame. Their continuation
 * loses register provenance, retaining only explicitly saved caller values.
 * This is bounded candidate discovery, not a whole-program alias proof. */
static int
pointer_path(const unsigned char *bytes, const unsigned char *kind,
    const unsigned char *width, unsigned end, unsigned pc, pointer_use_t state,
    unsigned nesting, unsigned *budget)
{
	while (*budget && pc >= 0x100 && pc < end && kind[pc] == 1 && width[pc]) {
		unsigned op = bytes[pc], next = pc+width[pc], pair = (op >> 4) & 3, reg = (op >> 3) & 7;
		unsigned destination = width[pc] == 3 ? bytes[pc+1] | (bytes[pc+2] << 8) : 0;
		--*budget;
		if ((op >= 0x40 && op < 0x80 && op != 0x76 && ((op & 7) == 6 || reg == 6)) ||
		    (op >= 0x80 && op < 0xc0 && (op & 7) == 6) ||
		    op == 0x34 || op == 0x35 || op == 0x36 || op == 0xe9 || op == 0xf9) {
			if (pointer_pair(&state, 2)) return 1;
		}
		if ((op == 2 || op == 0x0a) && pointer_pair(&state, 0)) return 1;
		if ((op == 0x12 || op == 0x1a) && pointer_pair(&state, 1)) return 1;
		if (op == 0xc3 || (op >= 0xc0 && (op & 7) == 2)) {
			if (op == 0xc3) { pc = destination; continue; }
			if (pointer_path(bytes, kind, width, end, destination, state, nesting, budget)) return 1;
		} else if (op == 0xcd || (op >= 0xc0 && (op & 7) == 4)) {
			if (destination == 5 && pointer_pair(&state, 1) &&
			    (state.service == 9 || state.service == 10 || state.service == 26 ||
			    (state.service >= 15 && state.service <= 23) || state.service == 30 ||
			    (state.service >= 33 && state.service <= 36) || state.service == 40)) return 1;
			if (nesting < 8 && state.depth < 16) {
				pointer_use_t callee = state;
				callee.stack[callee.depth][0] = callee.stack[callee.depth][1] = 0;
				++callee.depth;
				if (pointer_path(bytes, kind, width, end, destination, callee, nesting+1, budget)) return 1;
			}
			for (unsigned i = 0; i < 8; ++i) state.reg[i] = 0;
			state.service = -1;
		} else if (op == 0xc9 || op == 0xe9 || op == 0x76 || (op >= 0xc0 && (op & 7) == 7)) return 0;
		else if (op >= 0x40 && op < 0x80) {
			if (reg != 6) state.reg[reg] = state.reg[op & 7];
			if (reg == 1) state.service = -1;
		} else if (op < 0x40 && (op & 15) == 1) {
			if (pair < 3) state.reg[pair*2] = state.reg[pair*2+1] = 0;
			else state.depth = 0;
			if (!pair) state.service = bytes[pc+1];
		} else if (op < 0x40 && (op & 7) >= 4 && (op & 7) <= 6) {
			state.reg[reg] = 0;
			if (reg == 1) state.service = (op & 7) == 6 ? bytes[pc+1] : -1;
		} else if (op == 0xf9 || op == 0x33 || op == 0x3b) state.depth = 0;
		else if (op == 0xeb) {
			for (unsigned i = 0; i < 2; ++i) {
				unsigned char old = state.reg[2+i]; state.reg[2+i] = state.reg[4+i]; state.reg[4+i] = old;
			}
		} else if (op >= 0xc0 && (op & 15) == 5) {
			if (state.depth == 16) return 0;
			for (unsigned i = 0; i < 2; ++i) state.stack[state.depth][i] = pair < 3 ? state.reg[pair*2+i] : 0;
			++state.depth;
		} else if (op >= 0xc0 && (op & 15) == 1) {
			if (state.depth) --state.depth;
			else return 0;
			if (pair < 3) for (unsigned i = 0; i < 2; ++i) state.reg[pair*2+i] = state.stack[state.depth][i];
			else state.reg[7] = 0;
			if (!pair) state.service = -1;
		} else if (op == 0xe3) {
			if (!state.depth) return 0;
			for (unsigned i = 0; i < 2; ++i) {
				unsigned char old = state.reg[4+i]; state.reg[4+i] = state.stack[state.depth-1][i]; state.stack[state.depth-1][i] = old;
			}
		} else if (op == 0x2a || (op < 0x40 && (op & 15) == 9)) state.reg[4] = state.reg[5] = 0;
		else if ((op >= 0x80 && op < 0xc0) || (op >= 0xc0 && (op & 7) == 6) ||
		    op == 0x0a || op == 0x1a || op == 0x3a || op == 0xdb ||
		    op == 7 || op == 15 || op == 23 || op == 31 || op == 0x27 || op == 0x2f) state.reg[7] = 0;
		if (op == 3 || op == 0x0b) state.service = -1;
		pc = next;
	}
	return 0;
}

int
com_ptr_used(const unsigned char *bytes, const unsigned char *kind,
    const unsigned char *width, unsigned end, unsigned site)
{
	pointer_use_t state = { .service = -1 };
	unsigned budget = 512, pair = (bytes[site] >> 4) & 3;
	if (pair == 3) return 1; /* LXI SP explicitly loads a stack address. */
	state.reg[pair*2] = 1; state.reg[pair*2+1] = 2;
	return pointer_path(bytes, kind, width, end, site+3, state, 0, &budget);
}
