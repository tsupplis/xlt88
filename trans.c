/*
 * Copyright (c) 2021 Brian Callahan <bcallah@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "trans.h"
#include "osrecog.h"
#include "fcb.h"

typedef struct translation {
	const emitter_t *out;
	FILE *diagnostics;
	const backend_t *target;
	statement_t statement;
	const symbol_table_t *symbols;
	const source_program_t *source;
	unsigned char *boundaries;
	unsigned long source_line;
	const source_record_t *record;
	int errors;
	int label_number;
	int origin_seen;
	int image_started;
	long bdos_function;
	int fcb_call, grow_fcb;
} translation_t;

static void
diagnose(translation_t *ctx, const char *message, const char *detail)
{
	char text[768];
	snprintf(text, sizeof(text), "%s%s", message, detail);
	if (ctx->record) source_report(ctx->diagnostics, ctx->record, text);
	else fprintf(ctx->diagnostics, "xlt88:%lu: %s\n", ctx->source_line, text);
	emit(ctx->out, output_error_comment, NULL, value(operand_immediate, ctx->source_line), no_operand);
	++ctx->errors;
}

static int
equal_name(const char *left, const char *right)
{
	return symbol_equal(left, right);
}

static long
number(const char *text)
{
	return symbol_number(text);
}

static const char *
resolve_alias(translation_t *ctx, const char *text)
{
	return symbol_alias(ctx->symbols, text);
}

static long
address(translation_t *ctx, const char *text)
{
	return symbol_value(ctx->symbols, text);
}

static int
is_label(const char *text)
{
	if (!isalpha((unsigned char)*text) && *text != '_' && *text != '.')
		return 0;
	for (++text; *text; ++text)
		if (!isalnum((unsigned char)*text) && strchr("_.$?@#", *text) == NULL)
			return 0;
	return 1;
}

/* Resolve source names before the emitter applies assembler syntax. */
static const char *
canonical_name(void *context, const char *name, int *is_equ, size_t *id)
{
	const translation_t *ctx = context;
	const symbol_t *symbol = symbol_find(ctx->symbols, name);
	*is_equ = symbol != NULL && symbol->is_equ;
	if (id) *id = symbol ? symbol->id : 0;
	return symbol != NULL ? symbol->preferred : NULL;
}

static operand_t
reg(const char *name)
{
	return operand(operand_register, name);
}

static operand_t
expr(translation_t *ctx, const char *text)
{
	operand_t operand = operand(operand_expression, text);
	operand.resolve = canonical_name;
	operand.symbols = ctx;
	operand.source_line = ctx->source_line;
	return operand;
}

static operand_t
register_operand(const char *name, int wide)
{
	static const char *const narrow[] = {"al", "ch", "cl", "dh", "dl", "bh", "bl"};
	const size_t length = strcspn(name, " \t");
	const int letter = tolower((unsigned char)*name);

	if (wide) {
		if (length == 1) {
			switch (letter) {
			case 'b': return reg("cx");
			case 'd': return reg("dx");
			case 'h': return reg("bx");
			}
		}
		if (length == 2 && (name[0] == 'S' || name[0] == 's') &&
		    (name[1] == 'P' || name[1] == 'p'))
			return reg("sp");
		if (length == 3 && (!strncmp(name, "PSW", 3) || !strncmp(name, "psw", 3)))
			return reg("ax");
	} else if (length == 1) {
		if (letter >= 'a' && letter <= 'e')
			return reg(narrow[letter - 'a']);
		if (letter == 'h') return reg("bh");
		if (letter == 'l') return reg("bl");
		if (letter == 'm') return operand(operand_byte_memory, "bx");
	}
	return operand(operand_memory, name);
}

static void
nop(translation_t *ctx)
{
	instruction(ctx->out, "nop", no_operand, no_operand);
}

static void
lxi(translation_t *ctx)
{
	if (equal_name(ctx->statement.first, "sp") && ctx->target->stack_segment != NULL)
		ctx->target->stack_segment(ctx->out);
	instruction(ctx->out, "mov", register_operand(ctx->statement.first, 1), expr(ctx, ctx->statement.second));
}

static void
stax(translation_t *ctx)
{
	instruction(ctx->out, "mov", reg("di"), register_operand(ctx->statement.first, 1));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "mov", operand(operand_memory, "di"), reg("al"));
}

static void
inx(translation_t *ctx)
{
	instruction(ctx->out, "lahf", no_operand, no_operand);
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "inc", register_operand(ctx->statement.first, 1), no_operand);
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "sahf", no_operand, no_operand);
}

static void
inr(translation_t *ctx)
{
	instruction(ctx->out, "inc", register_operand(ctx->statement.first, 0), no_operand);
}

/* x86 AF records a nibble borrow, while 8080 AC records its inverse. */
static void
invert_auxiliary_carry(translation_t *ctx)
{
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "lahf", no_operand, no_operand);
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "xor", reg("ah"), value(operand_immediate, 16));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "sahf", no_operand, no_operand);
}

static void
dcr(translation_t *ctx)
{
	instruction(ctx->out, "dec", register_operand(ctx->statement.first, 0), no_operand);
	invert_auxiliary_carry(ctx);
}

static void
mvi(translation_t *ctx)
{
	instruction(ctx->out, "mov", register_operand(ctx->statement.first, 0), expr(ctx, ctx->statement.second));
}

static void
rlc(translation_t *ctx)
{
	instruction(ctx->out, "rol", reg("al"), value(operand_immediate, 1));
}

static void
dad(translation_t *ctx)
{
	instruction(ctx->out, "lahf", no_operand, no_operand);
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "add", reg("bx"), register_operand(ctx->statement.first, 1));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "rcr", reg("si"), value(operand_immediate, 1));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "sahf", no_operand, no_operand);
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "rcl", reg("si"), value(operand_immediate, 1));
}

static void
ldax(translation_t *ctx)
{
	instruction(ctx->out, "mov", reg("si"), register_operand(ctx->statement.first, 1));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "mov", reg("al"), operand(operand_memory, "si"));
}

static void
dcx(translation_t *ctx)
{
	instruction(ctx->out, "lahf", no_operand, no_operand);
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "dec", register_operand(ctx->statement.first, 1), no_operand);
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "sahf", no_operand, no_operand);
}

static void
rrc(translation_t *ctx)
{
	instruction(ctx->out, "ror", reg("al"), value(operand_immediate, 1));
}

static void
ral(translation_t *ctx)
{
	instruction(ctx->out, "rcl", reg("al"), value(operand_immediate, 1));
}

static void
rar(translation_t *ctx)
{
	instruction(ctx->out, "rcr", reg("al"), value(operand_immediate, 1));
}

static void
shld(translation_t *ctx)
{
	operand_t memory = expr(ctx, ctx->statement.first);
	memory.kind = operand_memory;
	instruction(ctx->out, "mov", memory, reg("bx"));
}

static void
daa(translation_t *ctx)
{
	const int label = ctx->label_number++;

	instruction(ctx->out, "push", reg("dx"), no_operand);
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "mov", reg("dl"), reg("al"));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "lahf", no_operand, no_operand);
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "mov", reg("dh"), reg("ah"));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "and", reg("dh"), value(operand_immediate, 17));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "mov", reg("ah"), reg("al"));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "and", reg("ah"), value(operand_immediate, 15));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "cmp", reg("ah"), value(operand_immediate, 9));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "ja", operand(operand_internal_label, "daa_low"), value(operand_immediate, label));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "test", reg("dh"), value(operand_immediate, 16));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "jz", operand(operand_internal_label, "daa_high"), value(operand_immediate, label));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	emit(ctx->out, output_internal_definition, "daa_low", value(operand_immediate, label), no_operand);
	instruction(ctx->out, "add", reg("al"), value(operand_immediate, 6));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	emit(ctx->out, output_internal_definition, "daa_high", value(operand_immediate, label), no_operand);
	instruction(ctx->out, "and", reg("dh"), value(operand_immediate, 1));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "cmp", reg("dl"), value(operand_immediate, 154));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "jae", operand(operand_internal_label, "daa_add"), value(operand_immediate, label));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "test", reg("dh"), value(operand_immediate, 1));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "jz", operand(operand_internal_label, "daa_flags"), value(operand_immediate, label));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	emit(ctx->out, output_internal_definition, "daa_add", value(operand_immediate, label), no_operand);
	instruction(ctx->out, "add", reg("al"), value(operand_immediate, 96));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "mov", reg("dh"), value(operand_immediate, 1));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	emit(ctx->out, output_internal_definition, "daa_flags", value(operand_immediate, label), no_operand);
	instruction(ctx->out, "xor", reg("dl"), reg("al"));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "and", reg("dl"), value(operand_immediate, 16));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "or", reg("dh"), reg("dl"));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "test", reg("al"), reg("al"));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "lahf", no_operand, no_operand);
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "and", reg("ah"), value(operand_immediate, 238));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "or", reg("ah"), reg("dh"));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "sahf", no_operand, no_operand);
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "pop", reg("dx"), no_operand);
}

static void
lhld(translation_t *ctx)
{
	operand_t memory = expr(ctx, ctx->statement.first);
	memory.kind = operand_memory;
	instruction(ctx->out, "mov", reg("bx"), memory);
}

static void
cma(translation_t *ctx)
{
	instruction(ctx->out, "not", reg("al"), no_operand);
}

static void
sta(translation_t *ctx)
{
	operand_t memory = expr(ctx, ctx->statement.first);
	memory.kind = operand_memory;
	instruction(ctx->out, "mov", memory, reg("al"));
}

static void
stc(translation_t *ctx)
{
	instruction(ctx->out, "stc", no_operand, no_operand);
}

static void
lda(translation_t *ctx)
{
	operand_t memory = expr(ctx, ctx->statement.first);
	memory.kind = operand_memory;
	instruction(ctx->out, "mov", reg("al"), memory);
}

static void
cmc(translation_t *ctx)
{
	instruction(ctx->out, "cmc", no_operand, no_operand);
}

static void
mov(translation_t *ctx)
{
	instruction(ctx->out, "mov", register_operand(ctx->statement.first, 0), register_operand(ctx->statement.second, 0));
}

static void
hlt(translation_t *ctx)
{
	instruction(ctx->out, "hlt", no_operand, no_operand);
}

static void
add(translation_t *ctx)
{
	instruction(ctx->out, "add", reg("al"), register_operand(ctx->statement.first, 0));
}

static void
adc(translation_t *ctx)
{
	instruction(ctx->out, "adc", reg("al"), register_operand(ctx->statement.first, 0));
}

static void
sub(translation_t *ctx)
{
	instruction(ctx->out, "sub", reg("al"), register_operand(ctx->statement.first, 0));
	invert_auxiliary_carry(ctx);
}

static void
sbb(translation_t *ctx)
{
	instruction(ctx->out, "sbb", reg("al"), register_operand(ctx->statement.first, 0));
	invert_auxiliary_carry(ctx);
}

static void
emit_8080_and(translation_t *ctx, operand_t operand)
{
	instruction(ctx->out, "push", reg("dx"), no_operand);
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "mov", reg("ah"), operand);
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "mov", reg("dl"), reg("al"));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "or", reg("dl"), reg("ah"));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "shl", reg("dl"), value(operand_immediate, 1));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "and", reg("dl"), value(operand_immediate, 16));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "and", reg("al"), reg("ah"));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "lahf", no_operand, no_operand);
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "and", reg("ah"), value(operand_immediate, 239));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "or", reg("ah"), reg("dl"));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "sahf", no_operand, no_operand);
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "pop", reg("dx"), no_operand);
}

static void
ana(translation_t *ctx)
{
	emit_8080_and(ctx, register_operand(ctx->statement.first, 0));
}

static void
xra(translation_t *ctx)
{
	instruction(ctx->out, "xor", reg("al"), register_operand(ctx->statement.first, 0));
}

static void
ora(translation_t *ctx)
{
	instruction(ctx->out, "or", reg("al"), register_operand(ctx->statement.first, 0));
}

static void
cmp(translation_t *ctx)
{
	instruction(ctx->out, "cmp", reg("al"), register_operand(ctx->statement.first, 0));
	invert_auxiliary_carry(ctx);
}

static void
push(translation_t *ctx)
{
	if (!strcmp(ctx->statement.first, "PSW") || !strcmp(ctx->statement.first, "psw")) {
		instruction(ctx->out, "lahf", no_operand, no_operand);
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
		instruction(ctx->out, "xchg", reg("al"), reg("ah"));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
		instruction(ctx->out, "push", reg("ax"), no_operand);
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
		instruction(ctx->out, "xchg", reg("al"), reg("ah"));
	} else {
		instruction(ctx->out, "push", register_operand(ctx->statement.first, 1), no_operand);
	}
}

static void
newlab(translation_t *ctx)
{
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	emit(ctx->out, output_internal_definition, NULL, value(operand_immediate, ctx->label_number++), no_operand);
}

static void
adi(translation_t *ctx)
{
	instruction(ctx->out, "add", reg("al"), expr(ctx, ctx->statement.first));
}

static void
rst(translation_t *ctx)
{
	if (address(ctx, ctx->statement.first) == 0)
		ctx->target->exit_program(ctx->out);
	else
		diagnose(ctx, "unsupported 8080 restart vector: ", ctx->statement.first);
}

static void
ret(translation_t *ctx)
{
	instruction(ctx->out, "ret", no_operand, no_operand);
}

static void
rz(translation_t *ctx)
{
	instruction(ctx->out, "jnz", operand(operand_internal_label, NULL), value(operand_immediate, ctx->label_number));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	ret(ctx);
	newlab(ctx);
}

static long
source_boundary(translation_t *ctx, const char *text)
{
	long offset, result = -1;
	size_t i;
	long location = ctx->source_line && ctx->source_line <= ctx->symbols->address_count ?
	    ctx->symbols->addresses[ctx->source_line - 1] : -1;
	if (!symbols_eval(ctx->symbols, text, location, &offset) || offset < 0 || offset > 65535) return -1;
	for (i = 0; i < ctx->source->count; ++i) {
		const char *op = ctx->source->records[i].statement.opcode;
		if (!strcmp(op, "end")) break;
		if (ctx->symbols->addresses[i] != offset || !*op ||
		    !strcmp(op, "equ") || !strcmp(op, "org") || !strcmp(op, "align") ||
		    !strcmp(op, "db") || !strcmp(op, "dw") || !strcmp(op, "ds")) continue;
		if (result >= 0) return -1; /* Overlaid code is ambiguous. */
		result = (long)i;
	}
	return result;
}

static void
transfer(translation_t *ctx, int tail)
{
	const char *destination = resolve_alias(ctx, ctx->statement.first);
	long offset = destination != NULL ? number(destination) : -1;

	if (!ctx->statement.second[0] && (destination == NULL || (offset < 0 && !is_label(destination)))) {
		long bound = source_boundary(ctx, ctx->statement.first);
		if (bound >= 0) {
			instruction(ctx->out, tail ? "jmp" : "call", operand(operand_internal_label, "src"), value(operand_immediate, bound));
			return;
		}
	}
	if (ctx->statement.second[0] || destination == NULL || (offset < 0 && !is_label(destination))) {
		diagnose(ctx, "unsupported transfer target expression: ", ctx->statement.first);
		return;
	}
	if (offset > 0xffff) {
		diagnose(ctx, "transfer target exceeds a 16-bit address: ", ctx->statement.first);
		return;
	}

	switch (offset) {
	case 0:
		ctx->target->exit_program(ctx->out);
		break;
	case 5:
		if (ctx->bdos_function == 0) {
			ctx->target->exit_program(ctx->out);
		} else if (ctx->bdos_function < 0) {
			diagnose(ctx, "BDOS call needs a statically proven function in C: ", ctx->statement.first);
		} else if (!(ctx->fcb_call && ctx->target->file_call ?
		    ctx->target->file_call(ctx->out, ctx->bdos_function, ctx->source_line) :
		    ctx->target->bdos(ctx->out, ctx->bdos_function))) {
			fprintf(ctx->diagnostics, "xlt88:%lu: BDOS function %ld needs a native port for %s\n",
			    ctx->source_line, ctx->bdos_function, ctx->target->name);
			diagnose(ctx, "cannot translate BDOS transfer: ", ctx->statement.first);
		} else if (tail) {
			emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
			ret(ctx);
		}
		break;
	default:
		if (offset >= 0) {
			const symbol_t *binding = NULL;
			size_t i;
			int ambiguous = 0;
			for (i = 0; i < ctx->symbols->count; ++i) {
				const symbol_t *item = &ctx->symbols->items[i];
				if (!item->is_equ && item->source_address == offset) {
					if (binding) ambiguous = 1;
					binding = item;
				}
			}
			if (ambiguous) {
				diagnose(ctx, "numeric transfer matches multiple labels; use a source name: ", ctx->statement.first);
				return;
			}
			instruction(ctx->out, tail ? "jmp" : "call", binding ?
			    operand(operand_branch, binding->preferred) : value(operand_branch, offset), no_operand);
		}
		else {
			int is_equ;
			const char *name = canonical_name(ctx, destination, &is_equ, NULL);
			instruction(ctx->out, tail ? "jmp" : "call",
			    operand(operand_branch, name != NULL ? name : destination), no_operand);
		}
		break;
	}
}

static void
call(translation_t *ctx)
{
	transfer(ctx, 0);
}

static void
jmp(translation_t *ctx)
{
	transfer(ctx, 1);
}

static void
rnz(translation_t *ctx)
{
	instruction(ctx->out, "jz", operand(operand_internal_label, NULL), value(operand_immediate, ctx->label_number));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	ret(ctx);
	newlab(ctx);
}

static void
pop(translation_t *ctx)
{
	if (!strcmp(ctx->statement.first, "PSW") || !strcmp(ctx->statement.first, "psw")) {
		instruction(ctx->out, "pop", reg("ax"), no_operand);
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
		instruction(ctx->out, "xchg", reg("al"), reg("ah"));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
		instruction(ctx->out, "sahf", no_operand, no_operand);
	} else {
		instruction(ctx->out, "pop", register_operand(ctx->statement.first, 1), no_operand);
	}
}

static void
jnz(translation_t *ctx)
{
	instruction(ctx->out, "jz", operand(operand_internal_label, NULL), value(operand_immediate, ctx->label_number));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	jmp(ctx);
	newlab(ctx);
}

static void
cnz(translation_t *ctx)
{
	instruction(ctx->out, "jz", operand(operand_internal_label, NULL), value(operand_immediate, ctx->label_number));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	call(ctx);
	newlab(ctx);
}

static void
jz(translation_t *ctx)
{
	instruction(ctx->out, "jnz", operand(operand_internal_label, NULL), value(operand_immediate, ctx->label_number));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	jmp(ctx);
	newlab(ctx);
}

static void
cz(translation_t *ctx)
{
	instruction(ctx->out, "jnz", operand(operand_internal_label, NULL), value(operand_immediate, ctx->label_number));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	call(ctx);
	newlab(ctx);
}

static void
aci(translation_t *ctx)
{
	instruction(ctx->out, "adc", reg("al"), expr(ctx, ctx->statement.first));
}

static void
rnc(translation_t *ctx)
{
	instruction(ctx->out, "jnae", operand(operand_internal_label, NULL), value(operand_immediate, ctx->label_number));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	ret(ctx);
	newlab(ctx);
}

static void
jnc(translation_t *ctx)
{
	instruction(ctx->out, "jnae", operand(operand_internal_label, NULL), value(operand_immediate, ctx->label_number));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	jmp(ctx);
	newlab(ctx);
}

static void
out(translation_t *ctx)
{
	instruction(ctx->out, "out", expr(ctx, ctx->statement.first), reg("al"));
}

static void
cnc(translation_t *ctx)
{
	instruction(ctx->out, "jnae", operand(operand_internal_label, NULL), value(operand_immediate, ctx->label_number));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	call(ctx);
	newlab(ctx);
}

static void
sui(translation_t *ctx)
{
	instruction(ctx->out, "sub", reg("al"), expr(ctx, ctx->statement.first));
	invert_auxiliary_carry(ctx);
}

static void
in(translation_t *ctx)
{
	instruction(ctx->out, "in", reg("al"), expr(ctx, ctx->statement.first));
}

static void
rc(translation_t *ctx)
{
	instruction(ctx->out, "jnb", operand(operand_internal_label, NULL), value(operand_immediate, ctx->label_number));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	ret(ctx);
	newlab(ctx);
}

static void
jc(translation_t *ctx)
{
	instruction(ctx->out, "jnb", operand(operand_internal_label, NULL), value(operand_immediate, ctx->label_number));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	jmp(ctx);
	newlab(ctx);
}

static void
cc(translation_t *ctx)
{
	instruction(ctx->out, "jnb", operand(operand_internal_label, NULL), value(operand_immediate, ctx->label_number));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	call(ctx);
	newlab(ctx);
}

static void
sbi(translation_t *ctx)
{
	instruction(ctx->out, "sbb", reg("al"), expr(ctx, ctx->statement.first));
	invert_auxiliary_carry(ctx);
}

static void
rpo(translation_t *ctx)
{
	instruction(ctx->out, "jp", operand(operand_internal_label, NULL), value(operand_immediate, ctx->label_number));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	ret(ctx);
	newlab(ctx);
}

static void
jpo(translation_t *ctx)
{
	instruction(ctx->out, "jp", operand(operand_internal_label, NULL), value(operand_immediate, ctx->label_number));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	jmp(ctx);
	newlab(ctx);
}

static void
xthl(translation_t *ctx)
{
	instruction(ctx->out, "pop", reg("si"), no_operand);
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "xchg", reg("bx"), reg("si"));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	instruction(ctx->out, "push", reg("si"), no_operand);
}

static void
cpo(translation_t *ctx)
{
	instruction(ctx->out, "jp", operand(operand_internal_label, NULL), value(operand_immediate, ctx->label_number));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	call(ctx);
	newlab(ctx);
}

static void
ani(translation_t *ctx)
{
	emit_8080_and(ctx, expr(ctx, ctx->statement.first));
}

static void
rpe(translation_t *ctx)
{
	instruction(ctx->out, "jnp", operand(operand_internal_label, NULL), value(operand_immediate, ctx->label_number));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	ret(ctx);
	newlab(ctx);
}

static void
pchl(translation_t *ctx)
{
	instruction(ctx->out, "jmp", reg("bx"), no_operand);
}

static void
jpe(translation_t *ctx)
{
	instruction(ctx->out, "jnp", operand(operand_internal_label, NULL), value(operand_immediate, ctx->label_number));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	jmp(ctx);
	newlab(ctx);
}

static void
xchg(translation_t *ctx)
{
	instruction(ctx->out, "xchg", reg("bx"), reg("dx"));
}

static void
cpe(translation_t *ctx)
{
	instruction(ctx->out, "jnp", operand(operand_internal_label, NULL), value(operand_immediate, ctx->label_number));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	call(ctx);
	newlab(ctx);
}

static void
xri(translation_t *ctx)
{
	instruction(ctx->out, "xor", reg("al"), expr(ctx, ctx->statement.first));
}

static void
rp(translation_t *ctx)
{
	instruction(ctx->out, "js", operand(operand_internal_label, NULL), value(operand_immediate, ctx->label_number));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	ret(ctx);
	newlab(ctx);
}

static void
jp(translation_t *ctx)
{
	instruction(ctx->out, "js", operand(operand_internal_label, NULL), value(operand_immediate, ctx->label_number));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	jmp(ctx);
	newlab(ctx);
}

static void
di(translation_t *ctx)
{
	instruction(ctx->out, "cli", no_operand, no_operand);
}

static void
cp(translation_t *ctx)
{
	instruction(ctx->out, "js", operand(operand_internal_label, NULL), value(operand_immediate, ctx->label_number));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	call(ctx);
	newlab(ctx);
}

static void
ori(translation_t *ctx)
{
	instruction(ctx->out, "or", reg("al"), expr(ctx, ctx->statement.first));
}

static void
rm(translation_t *ctx)
{
	instruction(ctx->out, "jns", operand(operand_internal_label, NULL), value(operand_immediate, ctx->label_number));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	ret(ctx);
	newlab(ctx);
}

static void
sphl(translation_t *ctx)
{
	if (ctx->target->stack_segment != NULL)
		ctx->target->stack_segment(ctx->out);
	instruction(ctx->out, "mov", reg("sp"), reg("bx"));
}

static void
jm(translation_t *ctx)
{
	instruction(ctx->out, "jns", operand(operand_internal_label, NULL), value(operand_immediate, ctx->label_number));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	jmp(ctx);
	newlab(ctx);
}

static void
ei(translation_t *ctx)
{
	instruction(ctx->out, "sti", no_operand, no_operand);
}

static void
cm(translation_t *ctx)
{
	instruction(ctx->out, "jns", operand(operand_internal_label, NULL), value(operand_immediate, ctx->label_number));
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
	call(ctx);
	newlab(ctx);
}

static void
cpi(translation_t *ctx)
{
	instruction(ctx->out, "cmp", reg("al"), expr(ctx, ctx->statement.first));
	invert_auxiliary_carry(ctx);
}

static void
org(translation_t *ctx)
{
	long origin, pc = ctx->source_line <= ctx->symbols->address_count ?
	    ctx->symbols->addresses[ctx->source_line - 1] : -1;
	if (ctx->statement.label[0] || ctx->statement.second[0] ||
	    !symbols_eval(ctx->symbols, ctx->statement.first, pc, &origin) || origin < 0 || origin > 65535) {
		diagnose(ctx, "ORG needs a known unlabelled 16-bit source address: ", ctx->statement.first);
		return;
	}
	if (!ctx->origin_seen && !ctx->image_started) {
		if (origin != 0x100) diagnose(ctx, "initial ORG must be 100h; out-of-order regions need an explicit port: ", ctx->statement.first);
	} else if (pc < 0 || origin < pc) {
		diagnose(ctx, "backward/unknown ORG would overlap source regions: ", ctx->statement.first);
	} else {
		emit(ctx->out, output_origin, NULL, value(operand_immediate, origin), no_operand);
	}
	ctx->origin_seen = 1;
}

static void
equ(translation_t *ctx)
{
	emit(ctx->out, output_equate, NULL, expr(ctx, ctx->statement.first), no_operand);
}

static void
db(translation_t *ctx)
{
	emit(ctx->out, output_data_bytes, NULL, expr(ctx, ctx->statement.first), no_operand);
}

static void
dw(translation_t *ctx)
{
	emit(ctx->out, output_data_words, NULL, expr(ctx, ctx->statement.first), no_operand);
}

static void
ds(translation_t *ctx)
{
	emit(ctx->out, output_reserve_bytes, NULL, expr(ctx, ctx->grow_fcb ? "36" : ctx->statement.first), no_operand);
}

/*
 * Big switch of all 8080 opcodes and their 8086 translations.
 */
static const struct trans {
	const char *op80;
	void (*cb)(translation_t *);
} tab[] = {
	{ "nop", nop },
	{ "lxi", lxi },
	{ "stax", stax },
	{ "inx", inx },
	{ "inr", inr },
	{ "dcr", dcr },
	{ "mvi", mvi },
	{ "rlc", rlc },
	{ "dad", dad },
	{ "ldax", ldax },
	{ "dcx", dcx },
	{ "rrc", rrc },
	{ "ral", ral },
	{ "rar", rar },
	{ "shld", shld },
	{ "daa", daa },
	{ "lhld", lhld },
	{ "cma", cma },
	{ "sta", sta },
	{ "stc", stc },
	{ "lda", lda },
	{ "cmc", cmc },
	{ "mov", mov },
	{ "hlt", hlt },
	{ "add", add },
	{ "adc", adc },
	{ "sub", sub },
	{ "sbb", sbb },
	{ "ana", ana },
	{ "xra", xra },
	{ "ora", ora },
	{ "cmp", cmp },
	{ "rnz", rnz },
	{ "pop", pop },
	{ "jnz", jnz },
	{ "jmp", jmp },
	{ "cnz", cnz },
	{ "push", push },
	{ "adi", adi },
	{ "rst", rst },
	{ "rz", rz },
	{ "ret", ret },
	{ "jz", jz },
	{ "cz", cz },
	{ "call", call },
	{ "aci", aci },
	{ "rnc", rnc },
	{ "jnc", jnc },
	{ "out", out },
	{ "cnc", cnc },
	{ "sui", sui },
	{ "rc", rc },
	{ "jc", jc },
	{ "in", in },
	{ "cc", cc },
	{ "sbi", sbi },
	{ "rpo", rpo },
	{ "jpo", jpo },
	{ "xthl", xthl },
	{ "cpo", cpo },
	{ "ani", ani },
	{ "rpe", rpe },
	{ "pchl", pchl },
	{ "jpe", jpe },
	{ "xchg", xchg },
	{ "cpe", cpe },
	{ "xri", xri },
	{ "rp", rp },
	{ "jp", jp },
	{ "di", di },
	{ "cp", cp },
	{ "ori", ori },
	{ "rm", rm },
	{ "sphl", sphl },
	{ "jm", jm },
	{ "ei", ei },
	{ "cm", cm },
	{ "cpi", cpi },
	{ "org", org },
	{ "equ", equ },
	{ "db", db },
	{ "dw", dw },
	{ "ds", ds }
};

static void
translate(translation_t *ctx)
{
	size_t i;
	long alignment;
	if (!ctx->statement.label[0] && !ctx->statement.opcode[0]) {
		emit(ctx->out, output_comment, ctx->statement.comment, no_operand, no_operand);
		emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
		return;
	}

	/* Alignment is explicit and precedes the following data label. */
	if (!strcmp(ctx->statement.opcode, "align")) {
		alignment = address(ctx, ctx->statement.first);
		if (ctx->statement.label[0] || ctx->statement.second[0] || alignment < 1 || alignment > 256 ||
		    (alignment & (alignment - 1)) != 0)
			diagnose(ctx, "ALIGN requires an unlabelled power of two (1–256): ", ctx->statement.first);
		else {
			emit(ctx->out, output_alignment, NULL, value(operand_immediate, alignment), no_operand);
			emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
			ctx->image_started = 1;
		}
		return;
	}
	if (strcmp(ctx->statement.opcode, "org") && strcmp(ctx->statement.opcode, "equ") && (ctx->statement.opcode[0] || ctx->statement.label[0]))
		ctx->image_started = 1;
	if (ctx->statement.label[0]) {
		const symbol_t *label = symbol_find(ctx->symbols, ctx->statement.label);
		if (!label) {
			diagnose(ctx, "symbol table missing source definition: ", ctx->statement.label);
			return;
		}
		emit(ctx->out, output_label, label->preferred,
		    value(operand_immediate, strcmp(ctx->statement.opcode, "equ") != 0), no_operand);
	}

	if (ctx->statement.opcode[0] != '\0') {
		for (i = 0; i < sizeof(tab) / sizeof(tab[0]); i++) {
			if (!strcmp(ctx->statement.opcode, tab[i].op80)) {
				tab[i].cb(ctx);
				break;
			}
		}
		if (i == sizeof(tab) / sizeof(tab[0]))
			diagnose(ctx, "unsupported mnemonic or directive: ", ctx->statement.opcode);
	}

	if (ctx->statement.comment[0])
		emit(ctx->out, output_comment, ctx->statement.comment, no_operand, no_operand);
	emit(ctx->out, output_line_end, NULL, no_operand, no_operand);
}

static long
recognition_value(void *context, const char *text)
{
	return address(context, text);
}

static int
translate_program(const source_program_t *program, const symbol_table_t *symbols, const backend_t *backend,
    const emitter_t *emitter, FILE *diagnostics)
{
	translation_t translation = {
		.out = emitter, .diagnostics = diagnostics, .target = backend,
		.bdos_function = -1, .symbols = symbols, .source = program
	};
	translation_t *ctx = &translation;
	size_t i;
	long *functions = NULL;
	fcb_plan_t fcbs = {0};

	ctx->boundaries = calloc(program->count ? program->count : 1, 1);
	if (!ctx->boundaries) { diagnose(ctx, "cannot allocate boundary bindings", ""); goto cleanup; }
	for (i = 0; i < program->count; ++i) {
		const statement_t *stmt = &program->records[i].statement;
		const char *op = stmt->opcode;
		if (!strcmp(op, "end")) break;
		if (*op == 'j' || !strcmp(op, "call") || !strcmp(op, "cnz") || !strcmp(op, "cz") ||
		    !strcmp(op, "cnc") || !strcmp(op, "cc") || !strcmp(op, "cpo") || !strcmp(op, "cpe") || !strcmp(op, "cp") || !strcmp(op, "cm")) {
			long bound;
			const char *alias = resolve_alias(ctx, stmt->first);
			if (alias && (number(alias) >= 0 || is_label(alias))) continue;
			ctx->source_line = i + 1;
			bound = source_boundary(ctx, stmt->first);
			if (bound >= 0) ctx->boundaries[bound] = 1;
		}
	}
	ctx->source_line = 0;
	if (!recognize_os_calls(program, recognition_value, ctx, &functions)) {
		diagnose(ctx, "cannot allocate OS annotations", "");
		goto cleanup;
	}
	if (backend->file_call && !fcb_analyze(program, symbols, functions, &fcbs, diagnostics)) {
		++ctx->errors;
		goto cleanup;
	}
	emit(ctx->out, output_header, backend->name, no_operand, no_operand);
	for (i = 0; i < program->count; ++i) {
		const source_record_t *record = &program->records[i];
		ctx->source_line = record->line;
		ctx->record = record;
		ctx->statement = record->statement;
		if (ctx->boundaries[i]) emit(ctx->out, output_internal_definition, "src", value(operand_immediate, i), no_operand);
		ctx->bdos_function = functions[i];
		ctx->fcb_call = fcbs.calls && fcbs.calls[i];
		ctx->grow_fcb = fcbs.grow && fcbs.grow[i];
		if (record->problem)
			diagnose(ctx, record->problem, "");
		else if (!strcmp(ctx->statement.opcode, "end"))
			break;
		else
			translate(ctx);
	}
cleanup:
	fcb_destroy(&fcbs);
	free(functions);
	free(ctx->boundaries);
	return ctx->errors != 0;
}

int
lower_named(const source_program_t *source, const symbol_table_t *symbols, const backend_t *backend,
    target_program_t *target, FILE *diagnostics)
{
	emitter_t builder = target_builder(target);
	int result = translate_program(source, symbols, backend, &builder, diagnostics);
	if (target->failed) {
		char message[512];
		snprintf(message, sizeof(message), "cannot build target program: %s", target->error ? target->error : "allocation failure");
		if (target->error_line && target->error_line <= source->count)
			source_report(diagnostics, &source->records[target->error_line - 1], message);
		else fprintf(diagnostics, "xlt88: %s\n", message);
		return 1;
	}
	return result;
}

int
lower_8080(const source_program_t *source, const backend_t *backend,
    target_program_t *target, FILE *diagnostics)
{
	symbol_table_t symbols = {0};
	int result = 1;
	if (symbols_build(source, &symbols, diagnostics))
		result = lower_named(source, &symbols, backend, target, diagnostics);
	symbols_destroy(&symbols);
	return result;
}
