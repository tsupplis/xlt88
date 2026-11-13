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

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * 8088ify -- Intel 8080 CP/M to 8086 (8088) MS-DOS assembly translator
 * Written for PCjam 2021: https://pcjam.gitlab.io/
 */

#include "backend.h"
#include "dialect.h"

struct symbol {
	char name[256];
	char value[256];
	int is_equ;
	struct symbol *next;
};

struct translation {
	FILE *out;
	const struct backend *target;
	struct statement statement;
	struct symbol *symbols;
	unsigned long source_line;
	int errors;
	int label_number;
	int origin_seen;
	int image_started;
	long bdos_function;
	char register_operands[2][258];
};

static void
diagnose(struct translation *ctx, const char *message, const char *detail)
{
	fprintf(stderr, "xlt88:%lu: %s%s\n", ctx->source_line, message, detail);
	fprintf(ctx->out, "; xlt88 error on source line %lu\n", ctx->source_line);
	++ctx->errors;
}

/* Each input record fits in the dialect's fixed-size statement buffers. */
static int
read_line(struct translation *ctx, FILE *in, char *line)
{
	size_t len;
	int ch;

	if (fgets(line, 256, in) == NULL)
		return 0;
	++ctx->source_line;
	len = strlen(line);
	if (len == 255 && line[len - 1] != '\n') {
		while ((ch = fgetc(in)) != EOF && ch != '\n')
			;
		diagnose(ctx, "source line exceeds 254 characters", "");
		line[0] = '\0';
	} else {
		line[strcspn(line, "\r\n")] = '\0';
	}
	return 1;
}

static int
equal_name(const char *left, const char *right)
{
	while (*left && tolower((unsigned char)*left) ==
	    tolower((unsigned char)*right)) {
		++left;
		++right;
	}
	return *left == *right;
}

static long
number(const char *text)
{
	char *end;
	long value;
	size_t len = strlen(text);

	if (!isdigit((unsigned char)*text))
		return -1;
	if (text[len - 1] == 'h' || text[len - 1] == 'H') {
		value = strtol(text, &end, 16);
		return end == text + len - 1 ? value : -1;
	}
	value = strtol(text, &end,
	    text[0] == '0' && tolower((unsigned char)text[1]) == 'x' ? 16 : 10);
	return *end == '\0' ? value : -1;
}

static const struct symbol *
find_symbol(const struct translation *ctx, const char *name)
{
	const struct symbol *symbol;

	for (symbol = ctx->symbols; symbol != NULL; symbol = symbol->next)
		if (equal_name(name, symbol->name))
			return symbol;
	return NULL;
}

static const char *
resolve_alias(struct translation *ctx, const char *text)
{
	const struct symbol *symbol;
	unsigned int depth;

	/* Allow forward EQU aliases and bound chains containing cycles. */
	for (depth = 0; depth < 256; ++depth) {
		symbol = find_symbol(ctx, text);
		if (symbol == NULL || !symbol->is_equ)
			return text;
		text = symbol->value;
	}
	return NULL;
}

static long
address(struct translation *ctx, const char *text)
{
	const char *value = resolve_alias(ctx, text);

	return value != NULL ? number(value) : -1;
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

/* NASM permits a keyword as a symbol when it is escaped with '$'.
 * Address labels in expressions can be made scalar in the flat image:
 * their offset from the section start plus the program origin. */
static void
emit_expression(struct translation *ctx, const char *text)
{
	const struct symbol *symbol;
	const char *start;
	char token[256];
	size_t length;

	while (*text) {
		start = text++;
		if (*start == '\'' || *start == '"') {
			while (*text && *text++ != *start)
				;
			fwrite(start, 1, text - start, ctx->out);
		} else if (isalnum((unsigned char)*start) ||
		    strchr("_.$?@#", *start) != NULL) {
			while (*text && (isalnum((unsigned char)*text) ||
			    strchr("_.$?@#", *text) != NULL))
				++text;
			length = text - start;
			memcpy(token, start, length);
			token[length] = '\0';
			symbol = find_symbol(ctx, token);
			if (symbol != NULL && symbol->is_equ)
				fprintf(ctx->out, "$%s", symbol->name);
			else if (symbol != NULL)
				fprintf(ctx->out, "($%s - $$ + 100h)", symbol->name);
			else if (equal_name(token, "and"))
				fputc('&', ctx->out);
			else
				fputs(token, ctx->out);
		} else {
			fputc(*start, ctx->out);
		}
	}
}

static char *
eight(struct translation *ctx, const char *a)
{
	char b[256];
	char *c = ctx->register_operands[a == ctx->statement.second];
	size_t i;

	for (i = 0; i < sizeof(b); i++)
		b[i] = '\0';

	for (i = 0; i < strlen(a); i++) {
		if (a[i] == ' ' || a[i] == '\t')
			break;
		b[i] = a[i];
	}

	if (!strcmp(b, "A") || !strcmp(b, "a"))
		return "al";
	if (!strcmp(b, "B") || !strcmp(b, "b"))
		return "ch";
	if (!strcmp(b, "C") || !strcmp(b, "c"))
		return "cl";
	if (!strcmp(b, "D") || !strcmp(b, "d"))
		return "dh";
	if (!strcmp(b, "E") || !strcmp(b, "e"))
		return "dl";
	if (!strcmp(b, "H") || !strcmp(b, "h"))
		return "bh";
	if (!strcmp(b, "L") || !strcmp(b, "l"))
		return "bl";
	if (!strcmp(b, "M") || !strcmp(b, "m"))
		return "byte [bx]";

	c[0] = '[';
	for (i = 0; i < strlen(a); i++)
		c[i + 1] = a[i];
	c[i + 1] = ']';
	c[i + 2] = '\0';

	return c;
}

static char *
sixteen(struct translation *ctx, const char *a)
{
	char b[256];
	char *c = ctx->register_operands[0];
	size_t i;

	for (i = 0; i < sizeof(b); i++)
		b[i] = '\0';

	for (i = 0; i < strlen(a); i++) {
		if (a[i] == ' ' || a[i] == '\t')
			break;
		b[i] = a[i];
	}

	if (!strcmp(b, "B") || !strcmp(b, "b"))
		return "cx";
	if (!strcmp(b, "D") || !strcmp(b, "d"))
		return "dx";
	if (!strcmp(b, "H") || !strcmp(b, "h"))
		return "bx";
	if (!strcmp(b, "PSW") || !strcmp(b, "psw"))
		return "ax";
	if (!strcmp(b, "SP") || !strcmp(b, "sp"))
		return "sp";

	c[0] = '[';
	for (i = 0; i < strlen(a); i++)
		c[i + 1] = a[i];
	c[i + 1] = ']';
	c[i + 2] = '\0';

	return c;
}

static void
nop(struct translation *ctx)
{
	fprintf(ctx->out, "nop");
}

static void
lxi(struct translation *ctx)
{
	if (equal_name(ctx->statement.first, "sp") && ctx->target->stack_segment != NULL)
		ctx->target->stack_segment(ctx->out);
	fprintf(ctx->out, "mov\t%s, ", sixteen(ctx, ctx->statement.first));
	emit_expression(ctx, ctx->statement.second);
}

static void
stax(struct translation *ctx)
{
	fprintf(ctx->out, "mov\tdi, %s\n", sixteen(ctx, ctx->statement.first));
	fprintf(ctx->out, "\tmov\t[di], al");
}

static void
inx(struct translation *ctx)
{
	fprintf(ctx->out, "lahf\n");
	fprintf(ctx->out, "\tinc\t%s\n", sixteen(ctx, ctx->statement.first));
	fprintf(ctx->out, "\tsahf");
}

static void
inr(struct translation *ctx)
{
	fprintf(ctx->out, "inc\t%s", eight(ctx, ctx->statement.first));
}

static void
dcr(struct translation *ctx)
{
	fprintf(ctx->out, "dec\t%s", eight(ctx, ctx->statement.first));
}

static void
mvi(struct translation *ctx)
{
	fprintf(ctx->out, "mov\t%s, ", eight(ctx, ctx->statement.first));
	emit_expression(ctx, ctx->statement.second);
}

static void
rlc(struct translation *ctx)
{
	fprintf(ctx->out, "rol\tal, 1");
}

static void
dad(struct translation *ctx)
{
	fprintf(ctx->out, "lahf\n");
	fprintf(ctx->out, "\tadd\tbx, %s\n", sixteen(ctx, ctx->statement.first));
	fprintf(ctx->out, "\trcr\tsi, 1\n");
	fprintf(ctx->out, "\tsahf\n");
	fprintf(ctx->out, "\trcl\tsi, 1");
}

static void
ldax(struct translation *ctx)
{
	fprintf(ctx->out, "mov\tsi, %s\n", sixteen(ctx, ctx->statement.first));
	fprintf(ctx->out, "\tmov\tal, [si]");
}

static void
dcx(struct translation *ctx)
{
	fprintf(ctx->out, "lahf\n");
	fprintf(ctx->out, "\tdec\t%s\n", sixteen(ctx, ctx->statement.first));
	fprintf(ctx->out, "\tsahf");
}

static void
rrc(struct translation *ctx)
{
	fprintf(ctx->out, "ror\tal, 1");
}

static void
ral(struct translation *ctx)
{
	fprintf(ctx->out, "rcl\tal, 1");
}

static void
rar(struct translation *ctx)
{
	fprintf(ctx->out, "rcr\tal, 1");
}

static void
shld(struct translation *ctx)
{
	fputs("mov\t[", ctx->out);
	emit_expression(ctx, ctx->statement.first);
	fputs("], bx", ctx->out);
}

static void
daa(struct translation *ctx)
{
	fprintf(ctx->out, "daa");
}

static void
lhld(struct translation *ctx)
{
	fputs("mov\tbx, [", ctx->out);
	emit_expression(ctx, ctx->statement.first);
	fputs("]", ctx->out);
}

static void
cma(struct translation *ctx)
{
	fprintf(ctx->out, "not\tal");
}

static void
sta(struct translation *ctx)
{
	fputs("mov\t[", ctx->out);
	emit_expression(ctx, ctx->statement.first);
	fputs("], al", ctx->out);
}

static void
stc(struct translation *ctx)
{
	fprintf(ctx->out, "stc");
}

static void
lda(struct translation *ctx)
{
	fputs("mov\tal, [", ctx->out);
	emit_expression(ctx, ctx->statement.first);
	fputs("]", ctx->out);
}

static void
cmc(struct translation *ctx)
{
	fprintf(ctx->out, "cmc");
}

static void
mov(struct translation *ctx)
{
	fprintf(ctx->out, "mov\t%s, %s", eight(ctx, ctx->statement.first), eight(ctx, ctx->statement.second));
}

static void
hlt(struct translation *ctx)
{
	fprintf(ctx->out, "hlt");
}

static void
add(struct translation *ctx)
{
	fprintf(ctx->out, "add\tal, %s", eight(ctx, ctx->statement.first));
}

static void
adc(struct translation *ctx)
{
	fprintf(ctx->out, "adc\tal, %s", eight(ctx, ctx->statement.first));
}

static void
sub(struct translation *ctx)
{
	fprintf(ctx->out, "sub\tal, %s", eight(ctx, ctx->statement.first));
}

static void
sbb(struct translation *ctx)
{
	fprintf(ctx->out, "sbb\tal, %s", eight(ctx, ctx->statement.first));
}

static void
emit_8080_and(struct translation *ctx, const char *operand, int immediate)
{
	/* 8080 AC is bit 3 of the OR of the original operands. Merge it
	 * into the result flags from AND. Read the operand before using DL
	 * as scratch so ANA D and ANA E still see the original DE. */
	fputs("push\tdx\n\tmov\tah, ", ctx->out);
	if (immediate)
		emit_expression(ctx, operand);
	else
		fputs(operand, ctx->out);
	fputs("\n\tmov\tdl, al\n"
	    "\tor\tdl, ah\n"
	    "\tshl\tdl, 1\n"
	    "\tand\tdl, 10h\n"
	    "\tand\tal, ah\n"
	    "\tlahf\n"
	    "\tand\tah, 0EFh\n"
	    "\tor\tah, dl\n"
	    "\tsahf\n"
	    "\tpop\tdx", ctx->out);
}

static void
ana(struct translation *ctx)
{
	emit_8080_and(ctx, eight(ctx, ctx->statement.first), 0);
}

static void
xra(struct translation *ctx)
{
	fprintf(ctx->out, "xor\tal, %s", eight(ctx, ctx->statement.first));
}

static void
ora(struct translation *ctx)
{
	fprintf(ctx->out, "or\tal, %s", eight(ctx, ctx->statement.first));
}

static void
cmp(struct translation *ctx)
{
	fprintf(ctx->out, "cmp\tal, %s", eight(ctx, ctx->statement.first));
}

static void
push(struct translation *ctx)
{
	if (!strcmp(ctx->statement.first, "PSW") || !strcmp(ctx->statement.first, "psw")) {
		fprintf(ctx->out, "lahf\n");
		fprintf(ctx->out, "\txchg\tal, ah\n");
		fprintf(ctx->out, "\tpush\tax\n");
		fprintf(ctx->out, "\txchg\tal, ah");
	} else {
		fprintf(ctx->out, "push\t%s", sixteen(ctx, ctx->statement.first));
	}
}

static void
newlab(struct translation *ctx)
{
	fprintf(ctx->out, "\nL@%d:", ctx->label_number++);
}

static void
adi(struct translation *ctx)
{
	fputs("add\tal, ", ctx->out);
	emit_expression(ctx, ctx->statement.first);
}

static void
rst(struct translation *ctx)
{
	if (address(ctx, ctx->statement.first) == 0)
		ctx->target->exit_program(ctx->out);
	else
		diagnose(ctx, "unsupported 8080 restart vector: ", ctx->statement.first);
}

static void
ret(struct translation *ctx)
{
	fprintf(ctx->out, "ret");
}

static void
rz(struct translation *ctx)
{
	fprintf(ctx->out, "jnz\tL@%d\n\t", ctx->label_number);
	ret(ctx);
	newlab(ctx);
}

static void
transfer(struct translation *ctx, int tail)
{
	const char *destination = resolve_alias(ctx, ctx->statement.first);
	long offset = destination != NULL ? number(destination) : -1;

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
			diagnose(ctx, "BDOS call needs an immediately preceding MVI C, constant: ", ctx->statement.first);
		} else if (!ctx->target->bdos(ctx->out, ctx->bdos_function)) {
			fprintf(stderr, "xlt88:%lu: BDOS function %ld needs a native port for %s\n",
			    ctx->source_line, ctx->bdos_function, ctx->target->name);
			diagnose(ctx, "cannot translate BDOS transfer: ", ctx->statement.first);
		} else if (tail) {
			fputs("\n\tret", ctx->out);
		}
		break;
	default:
		fprintf(ctx->out, "%s\t", tail ? "jmp" : "call");
		if (offset >= 0) {
			fprintf(ctx->out, "0x%lx", offset);
		} else {
			const struct symbol *symbol = find_symbol(ctx, destination);
			fprintf(ctx->out, "$%s", symbol != NULL ? symbol->name : destination);
		}
		break;
	}
}

static void
call(struct translation *ctx)
{
	transfer(ctx, 0);
}

static void
jmp(struct translation *ctx)
{
	transfer(ctx, 1);
}

static void
rnz(struct translation *ctx)
{
	fprintf(ctx->out, "jz\tL@%d\n\t", ctx->label_number);
	ret(ctx);
	newlab(ctx);
}

static void
pop(struct translation *ctx)
{
	if (!strcmp(ctx->statement.first, "PSW") || !strcmp(ctx->statement.first, "psw")) {
		fprintf(ctx->out, "pop\tax\n");
		fprintf(ctx->out, "\txchg\tal, ah\n");
		fprintf(ctx->out, "\tsahf");
	} else {
		fprintf(ctx->out, "pop\t%s", sixteen(ctx, ctx->statement.first));
	}
}

static void
jnz(struct translation *ctx)
{
	fprintf(ctx->out, "jz\tL@%d\n\t", ctx->label_number);
	jmp(ctx);
	newlab(ctx);
}

static void
cnz(struct translation *ctx)
{
	fprintf(ctx->out, "jz\tL@%d\n\t", ctx->label_number);
	call(ctx);
	newlab(ctx);
}

static void
jz(struct translation *ctx)
{
	fprintf(ctx->out, "jnz\tL@%d\n\t", ctx->label_number);
	jmp(ctx);
	newlab(ctx);
}

static void
cz(struct translation *ctx)
{
	fprintf(ctx->out, "jnz\tL@%d\n\t", ctx->label_number);
	call(ctx);
	newlab(ctx);
}

static void
aci(struct translation *ctx)
{
	fputs("adc\tal, ", ctx->out);
	emit_expression(ctx, ctx->statement.first);
}

static void
rnc(struct translation *ctx)
{
	fprintf(ctx->out, "jnae\tL@%d\n\t", ctx->label_number);
	ret(ctx);
	newlab(ctx);
}

static void
jnc(struct translation *ctx)
{
	fprintf(ctx->out, "jnae\tL@%d\n\t", ctx->label_number);
	jmp(ctx);
	newlab(ctx);
}

static void
out(struct translation *ctx)
{
	fputs("out\t", ctx->out);
	emit_expression(ctx, ctx->statement.first);
	fputs(", al", ctx->out);
}

static void
cnc(struct translation *ctx)
{
	fprintf(ctx->out, "jnae\tL@%d\n\t", ctx->label_number);
	call(ctx);
	newlab(ctx);
}

static void
sui(struct translation *ctx)
{
	fputs("sub\tal, ", ctx->out);
	emit_expression(ctx, ctx->statement.first);
}

static void
in(struct translation *ctx)
{
	fputs("in\tal, ", ctx->out);
	emit_expression(ctx, ctx->statement.first);
}

static void
rc(struct translation *ctx)
{
	fprintf(ctx->out, "jnb\tL@%d\n\t", ctx->label_number);
	ret(ctx);
	newlab(ctx);
}

static void
jc(struct translation *ctx)
{
	fprintf(ctx->out, "jnb\tL@%d\n\t", ctx->label_number);
	jmp(ctx);
	newlab(ctx);
}

static void
cc(struct translation *ctx)
{
	fprintf(ctx->out, "jnb\tL@%d\n\t", ctx->label_number);
	call(ctx);
	newlab(ctx);
}

static void
sbi(struct translation *ctx)
{
	fputs("sbb\tal, ", ctx->out);
	emit_expression(ctx, ctx->statement.first);
}

static void
rpo(struct translation *ctx)
{
	fprintf(ctx->out, "jp\tL@%d\n\t", ctx->label_number);
	ret(ctx);
	newlab(ctx);
}

static void
jpo(struct translation *ctx)
{
	fprintf(ctx->out, "jp\tL@%d\n\t", ctx->label_number);
	jmp(ctx);
	newlab(ctx);
}

static void
xthl(struct translation *ctx)
{
	fprintf(ctx->out, "pop\tsi\n");
	fprintf(ctx->out, "\txchg\tbx, si\n");
	fprintf(ctx->out, "\tpush\tsi");
}

static void
cpo(struct translation *ctx)
{
	fprintf(ctx->out, "jp\tL@%d\n\t", ctx->label_number);
	call(ctx);
	newlab(ctx);
}

static void
ani(struct translation *ctx)
{
	emit_8080_and(ctx, ctx->statement.first, 1);
}

static void
rpe(struct translation *ctx)
{
	fprintf(ctx->out, "jnp\tL@%d\n\t", ctx->label_number);
	ret(ctx);
	newlab(ctx);
}

static void
pchl(struct translation *ctx)
{
	fprintf(ctx->out, "jmp\tbx");
}

static void
jpe(struct translation *ctx)
{
	fprintf(ctx->out, "jnp\tL@%d\n\t", ctx->label_number);
	jmp(ctx);
	newlab(ctx);
}

static void
xchg(struct translation *ctx)
{
	fprintf(ctx->out, "xchg\tbx, dx");
}

static void
cpe(struct translation *ctx)
{
	fprintf(ctx->out, "jnp\tL@%d\n\t", ctx->label_number);
	call(ctx);
	newlab(ctx);
}

static void
xri(struct translation *ctx)
{
	fputs("xor\tal, ", ctx->out);
	emit_expression(ctx, ctx->statement.first);
}

static void
rp(struct translation *ctx)
{
	fprintf(ctx->out, "js\tL@%d\n\t", ctx->label_number);
	ret(ctx);
	newlab(ctx);
}

static void
jp(struct translation *ctx)
{
	fprintf(ctx->out, "js\tL@%d\n\t", ctx->label_number);
	jmp(ctx);
	newlab(ctx);
}

static void
di(struct translation *ctx)
{
	fprintf(ctx->out, "cli");
}

static void
cp(struct translation *ctx)
{
	fprintf(ctx->out, "js\tL@%d\n\t", ctx->label_number);
	call(ctx);
	newlab(ctx);
}

static void
ori(struct translation *ctx)
{
	fputs("or\tal, ", ctx->out);
	emit_expression(ctx, ctx->statement.first);
}

static void
rm(struct translation *ctx)
{
	fprintf(ctx->out, "jns\tL@%d\n\t", ctx->label_number);
	ret(ctx);
	newlab(ctx);
}

static void
sphl(struct translation *ctx)
{
	if (ctx->target->stack_segment != NULL)
		ctx->target->stack_segment(ctx->out);
	fprintf(ctx->out, "mov\tsp, bx");
}

static void
jm(struct translation *ctx)
{
	fprintf(ctx->out, "jns\tL@%d\n\t", ctx->label_number);
	jmp(ctx);
	newlab(ctx);
}

static void
ei(struct translation *ctx)
{
	fprintf(ctx->out, "sti");
}

static void
cm(struct translation *ctx)
{
	fprintf(ctx->out, "jns\tL@%d\n\t", ctx->label_number);
	call(ctx);
	newlab(ctx);
}

static void
cpi(struct translation *ctx)
{
	fputs("cmp\tal, ", ctx->out);
	emit_expression(ctx, ctx->statement.first);
}

static void
org(struct translation *ctx)
{
	if (ctx->origin_seen || ctx->image_started || ctx->statement.label[0] || address(ctx, ctx->statement.first) != 0x100)
		diagnose(ctx, "expected a single initial ORG 100h, got ", ctx->statement.first);
	ctx->origin_seen = 1;
}

static void
equ(struct translation *ctx)
{
	fputs("equ\t", ctx->out);
	emit_expression(ctx, ctx->statement.first);
}

static void
db(struct translation *ctx)
{
	fputs("db\t", ctx->out);
	emit_expression(ctx, ctx->statement.first);
}

static void
dw(struct translation *ctx)
{
	fputs("dw\t", ctx->out);
	emit_expression(ctx, ctx->statement.first);
}

static void
ds(struct translation *ctx)
{
	/* Include the reserved bytes in the flat image for both loaders. */
	fputs("times (", ctx->out);
	emit_expression(ctx, ctx->statement.first);
	fputs(") db 0", ctx->out);
}

/*
 * Big switch of all 8080 opcodes and their 8086 translations.
 */
static const struct trans {
	const char *op80;
	void (*cb)(struct translation *);
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
translate(struct translation *ctx)
{
	size_t i;
	long alignment;
	long next_function = -1;

	/* A labelled transfer may be entered without running the preceding
	 * instruction. Only use a constant from straight-line fallthrough. */
	if (ctx->statement.label[0] || !strcmp(ctx->statement.opcode, "align"))
		ctx->bdos_function = -1;
	if (!ctx->statement.label[0] && !ctx->statement.opcode[0]) {
		fprintf(ctx->out, "%s\n", ctx->statement.comment);
		return;
	}
	if (!strcmp(ctx->statement.opcode, "mvi") && equal_name(ctx->statement.first, "c"))
		next_function = address(ctx, ctx->statement.second);

	/* Alignment is explicit and precedes the following data label. */
	if (!strcmp(ctx->statement.opcode, "align")) {
		alignment = address(ctx, ctx->statement.first);
		if (ctx->statement.label[0] || ctx->statement.second[0] || alignment < 1 || alignment > 256 ||
		    (alignment & (alignment - 1)) != 0)
			diagnose(ctx, "ALIGN requires an unlabelled power of two (1–256): ", ctx->statement.first);
		else {
			fprintf(ctx->out, "\talign %ld, db 0\n", alignment);
			ctx->image_started = 1;
		}
		return;
	}
	if (strcmp(ctx->statement.opcode, "org") && strcmp(ctx->statement.opcode, "equ") && (ctx->statement.opcode[0] || ctx->statement.label[0]))
		ctx->image_started = 1;
	if (ctx->statement.label[0] != '\0') {
		fprintf(ctx->out, "$%s", ctx->statement.label);
		if (!!strcmp(ctx->statement.opcode, "equ"))
			fputc(':', ctx->out);
	}

	if (ctx->statement.opcode[0] != '\0') {
		for (i = 0; i < sizeof(tab) / sizeof(tab[0]); i++) {
			if (!strcmp(ctx->statement.opcode, tab[i].op80)) {
				fputc('\t', ctx->out);
				tab[i].cb(ctx);
				break;
			}
		}
		if (i == sizeof(tab) / sizeof(tab[0]))
			diagnose(ctx, "unsupported mnemonic or directive: ", ctx->statement.opcode);
	}

	if (ctx->statement.comment[0] != '\0') {
		if (ctx->statement.label[0] != '\0' || ctx->statement.opcode[0] != '\0')
			fputc('\t', ctx->out);
		fprintf(ctx->out, "%s", ctx->statement.comment);
	}
	fputc('\n', ctx->out);
	ctx->bdos_function = next_function;
}

static void
assemble(struct translation *ctx, FILE *in, const struct dialect *dialect)
{
	char line[256];
	const char *problem;
	struct symbol *item;

	/* Collect labels and EQU definitions for forward references. */
	while (read_line(ctx, in, line)) {
		problem = dialect->parse(line, &ctx->statement);
		if (problem == NULL && !strcmp(ctx->statement.opcode, "end"))
			break;
		if (problem != NULL || !ctx->statement.label[0])
			continue;
		if (find_symbol(ctx, ctx->statement.label) != NULL) {
			diagnose(ctx, "duplicate source symbol: ", ctx->statement.label);
			goto cleanup;
		}
		item = malloc(sizeof(*item));
		if (item == NULL) {
			diagnose(ctx, "cannot allocate symbol table", "");
			goto cleanup;
		}
		strcpy(item->name, ctx->statement.label);
		strcpy(item->value, ctx->statement.first);
		item->is_equ = !strcmp(ctx->statement.opcode, "equ");
		item->next = ctx->symbols;
		ctx->symbols = item;
	}
	if (ferror(in) || fseek(in, 0, SEEK_SET) != 0) {
		diagnose(ctx, "cannot reread source file", "");
		goto cleanup;
	}

	ctx->source_line = 0;
	fprintf(ctx->out, "bits 16\ncpu 8086\norg 100h\n; Target: %s\n", ctx->target->name);
	while (read_line(ctx, in, line)) {
		problem = dialect->parse(line, &ctx->statement);
		if (problem != NULL)
			diagnose(ctx, problem, "");
		else if (!strcmp(ctx->statement.opcode, "end"))
			break;
		else
			translate(ctx);
	}
	if (ferror(in))
		diagnose(ctx, "cannot read source file", "");
cleanup:
	while (ctx->symbols != NULL) {
		item = ctx->symbols;
		ctx->symbols = item->next;
		free(item);
	}
}

static FILE *
open_output(FILE *input, const char *path)
{
	struct stat input_info, output_info;
	FILE *output;
	int fd;

	if (fstat(fileno(input), &input_info) < 0) {
		perror("xlt88: input");
		return NULL;
	}
	/* Check the opened file before truncating it. Path comparison alone
	 * misses hard links and symlinks, including ones changed during open. */
	fd = open(path, O_WRONLY | O_CREAT, 0666);
	if (fd < 0) {
		perror(path);
		return NULL;
	}
	if (fstat(fd, &output_info) < 0) {
		perror(path);
	} else if (input_info.st_dev == output_info.st_dev &&
	    input_info.st_ino == output_info.st_ino) {
		fputs("xlt88: input and output must differ\n", stderr);
	} else if (S_ISREG(output_info.st_mode) && ftruncate(fd, 0) < 0) {
		perror(path);
	} else {
		output = fdopen(fd, "w");
		if (output != NULL)
			return output;
		perror(path);
	}
	close(fd);
	return NULL;
}

int
main(int argc, char *argv[])
{
	struct translation ctx = { .target = &msdos_backend, .bdos_function = -1 };
	const struct dialect *dialect = &intel_dialect;
	const char *input, *output;
	FILE *fp;
	int arg = 1;

	while (arg < argc && argv[arg][0] == '-') {
		if (!strcmp(argv[arg], "--target=msdos"))
			ctx.target = &msdos_backend;
		else if (!strcmp(argv[arg], "--target=cpm86"))
			ctx.target = &cpm86_backend;
		else if (!strcmp(argv[arg], "--dialect=intel"))
			dialect = &intel_dialect;
		else if (!strcmp(argv[arg], "--")) {
			++arg;
			break;
		} else
			goto usage;
		++arg;
	}
	if (argc - arg != 2)
		goto usage;
	input = argv[arg];
	output = argv[arg + 1];
	if (!strcmp(input, output)) {
		fputs("xlt88: input and output must differ\n", stderr);
		return 1;
	}
	fp = fopen(input, "r");
	if (fp == NULL) {
		perror(input);
		return 1;
	}
	ctx.out = open_output(fp, output);
	if (ctx.out == NULL) {
		fclose(fp);
		return 1;
	}

	assemble(&ctx, fp, dialect);
	fclose(fp);
	if (fclose(ctx.out) != 0) {
		perror(output);
		return 1;
	}
	return ctx.errors != 0;

usage:
	fputs("usage: xlt88 [--target=msdos|cpm86] [--dialect=intel]"
	    " infile.asm outfile.asm\n", stderr);
	return 1;
}
