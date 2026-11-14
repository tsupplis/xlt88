#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "asmfmt.h"

typedef struct asm_name {
	const char *source;
	char name[32];
	const expr_t *equ;
	int written;
} asm_name_t;
typedef struct asm_context {
	FILE *out;
	int masm, relocatable;
	int memory_byte;
	asm_name_t *names;
	size_t count;
	unsigned long branch, memory, location_count;
	unsigned eval_depth;
	size_t current_equ;
	const char *location;
	int failed;
} asm_context_t;

static const asm_name_t *
find_name(const asm_context_t *ctx, const char *name)
{
	size_t i;
	for (i = 0; i < ctx->count; ++i)
		if (!strcmp(ctx->names[i].source, name)) return &ctx->names[i];
	return NULL;
}

static int
lookup_equ(void *context, size_t id, const char *name, long *value)
{
	asm_context_t *ctx = context;
	const asm_name_t *item = find_name(ctx, name);
	int result;
	(void)id;
	if (!item || !item->equ || ctx->eval_depth >= 128) return 0;
	++ctx->eval_depth;
	result = expr_eval(item->equ, lookup_equ, ctx, -1, value);
	--ctx->eval_depth;
	return result;
}

static void
write_name(asm_context_t *ctx, const char *name)
{
	const asm_name_t *item = find_name(ctx, name);
	if (item) fputs(item->name, ctx->out);
	else { ctx->failed = 1; fputs("undefined", ctx->out); }
}

static void
write_expr(asm_context_t *ctx, const expr_t *tree)
{
	static const char *const operators[] = {
		"+", "-", "*", "/", "mod", "and", "or", "xor", "shl", "shr", "not",
		"eq", "ne", "lt", "le", "gt", "ge"
	};
	FILE *out = ctx->out;
	long constant;
	if (!tree) { ctx->failed = 1; return; }
	if (tree->kind != expr_string && tree->kind != expr_list &&
	    expr_eval(tree, lookup_equ, ctx, -1, &constant)) {
		if (constant < -32768 || constant > 65535) ctx->failed = 1;
		fprintf(out, "%ld", constant);
		return;
	}
	switch (tree->kind) {
	case expr_integer: fprintf(out, "%ld", tree->integer); break;
	case expr_string: fputs(tree->text, out); break;
	case expr_location:
		if (ctx->location) {
			if (ctx->relocatable) fprintf(out, "(%s-x_entry+100h)", ctx->location);
			else fputs(ctx->location, out);
		}
		else fputs((ctx->masm || ctx->relocatable) ? "($-x_entry+100h)" : "(offset $)", out);
		break;
	case expr_symbol:
		if (ctx->masm || ctx->relocatable) {
			fputc('(', out); write_name(ctx, tree->text);
			fputs(tree->is_equ ? ")" : "-x_entry+100h)", out);
		} else {
			fputs("(offset ", out); write_name(ctx, tree->text); fputc(')', out);
		}
		break;
	case expr_unary:
		fprintf(out, "(%s ", operators[tree->op]); write_expr(ctx, tree->left); fputc(')', out); break;
	case expr_binary:
		fputc('(', out); write_expr(ctx, tree->left);
		fprintf(out, " %s ", operators[tree->op]); write_expr(ctx, tree->right); fputc(')', out); break;
	case expr_list:
		write_expr(ctx, tree->left); fputc(',', out); write_expr(ctx, tree->right); break;
	}
}

static void
internal_label(FILE *out, const char *prefix, long number)
{
	fprintf(out, "i_%s%ld", prefix ? prefix : "", number);
}

static void
write_operand(asm_context_t *ctx, const operand_t *item)
{
	FILE *out = ctx->out;
	switch (item->kind) {
	case operand_register: fputs(item->text, out); break;
	case operand_interrupt: case operand_immediate: fprintf(out, "%ld", item->value); break;
	case operand_expression:
		if (ctx->masm) fputs("offset ", out);
		write_expr(ctx, item->expression); break;
	case operand_branch:
		if (item->text) write_name(ctx, item->text);
		else fprintf(out, "%ld", item->value);
		break;
	case operand_byte_memory: case operand_memory:
		if (item->expression) {
			if (ctx->masm) fputs(ctx->memory_byte || item->kind == operand_byte_memory ? "byte ptr ds:" : "word ptr ds:", out);
			else if (item->kind == operand_byte_memory) fputs("byte ptr ", out);
			fprintf(out, ctx->masm ? "[m_%lu]" : ".m_%lu", ++ctx->memory);
		} else {
			fputs(item->kind == operand_byte_memory ? "byte ptr [" : "[", out);
			fputs(item->text, out); fputc(']', out);
		}
		break;
	case operand_internal_label: break;
	}
}

static void
write_arguments(asm_context_t *ctx, const output_t *event)
{
	const operand_t *first = &event->operands[0], *second = &event->operands[1];
	ctx->memory_byte = (first->kind == operand_register && first->text && strlen(first->text) == 2 && (first->text[1] == 'l' || first->text[1] == 'h')) ||
	    (second->kind == operand_register && second->text && strlen(second->text) == 2 && (second->text[1] == 'l' || second->text[1] == 'h'));
	if (first->kind == operand_internal_label) {
		fputc(' ', ctx->out); internal_label(ctx->out, first->text, second->value);
	} else if (first->kind != operand_register || first->text) {
		fputc(' ', ctx->out); write_operand(ctx, first);
		if (second->kind != operand_register || second->text) {
			fputc(',', ctx->out); write_operand(ctx, second);
		}
	}
}

static int
has_location(const expr_t *tree)
{
	return tree && (tree->kind == expr_location || has_location(tree->left) || has_location(tree->right));
}

static void
write_asm86(void *context, const output_t *event)
{
	static const char *const jumps[] = {"jz", "jnz", "jc", "jnc", "jp", "jnp", "js", "jns", "jo", "jno", "je", "jne", "ja", "jbe", "jae", "jb", "jnae", "jnb", "jpe", "jpo"};
	asm_context_t *ctx = context;
	FILE *out = ctx->out;
	const operand_t *first = &event->operands[0];
	size_t i;
	char location[32];
	ctx->location = NULL;
	if (ctx->relocatable && (event->kind == output_instruction ||
	    event->kind == output_data_bytes || event->kind == output_data_words ||
	    event->kind == output_reserve_bytes) &&
	    (has_location(event->operands[0].expression) || has_location(event->operands[1].expression))) {
		snprintf(location, sizeof(location), "p_%lu", ++ctx->location_count);
		fprintf(out, "%s:\n", location);
		ctx->location = location;
	}
	switch (event->kind) {
	case output_header:
		fprintf(out, "; xlt88 target: %s\n", event->text);
		fputs(ctx->masm ? "x_code segment para public 'CODE'\nassume cs:x_code,ds:x_code,es:x_code,ss:x_code\norg 100h\nx_entry:\n" : "\tcseg\n\torg 100h\n", out);
		if (ctx->relocatable) fputs("x_entry:\n", out);
		break;
	case output_instruction: {
		unsigned long memory = ctx->memory;
		for (i = 0; i < 2; ++i) {
			const operand_t *op = &event->operands[i];
			if ((op->kind == operand_memory || op->kind == operand_byte_memory) && op->expression)
				fprintf(out, ctx->relocatable ? "a_%lu:\n" : "a_%lu equ %s\n", ++memory, ctx->masm ? "($-x_entry+100h)" : "offset $");
		}
		/* ASM86 cannot relax out-of-range conditional jumps. */
		for (i = 0; i < sizeof(jumps)/sizeof(jumps[0]); ++i) if (!strcmp(event->text, jumps[i])) break;
		if (i < sizeof(jumps)/sizeof(jumps[0]) && !event->short_branch) {
			fprintf(out, "\t%s z_%lu\n\tjmp", jumps[i ^ 1], ++ctx->branch);
			if (ctx->masm) fputs(" near ptr", out);
			write_arguments(ctx, event);
			fprintf(out, "\nz_%lu:", ctx->branch);
		} else {
			fprintf(out, "\t%s", !ctx->masm && event->short_branch && !strcmp(event->text, "jmp") ? "jmps" : event->text);
			if (ctx->masm && !strcmp(event->text, "jmp") && (first->kind == operand_branch || first->kind == operand_internal_label)) fputs(event->short_branch ? " short" : " near ptr", out);
			write_arguments(ctx, event);
		}
		break;
	}
	case output_label:
		fprintf(out, "; source: %s\n", event->text);
		if (first->value) { write_name(ctx, event->text); fputs(":\n", out); }
		else ctx->current_equ = (size_t)(find_name(ctx, event->text) - ctx->names);
		break;
	case output_internal_definition: internal_label(out, event->text, first->value); fputs(":\n", out); break;
	case output_equate: {
		long constant;
		asm_name_t *item = &ctx->names[ctx->current_equ];
		if (expr_eval(first->expression, lookup_equ, ctx, -1, &constant)) {
			if (constant < -32768 || constant > 65535) ctx->failed = 1;
			fprintf(out, "%s equ %ld", item->name, constant); item->written = 1;
		} else fprintf(out, ctx->relocatable ? "e_%lu:" : "e_%lu equ %s", (unsigned long)ctx->current_equ, ctx->masm ? "($-x_entry+100h)" : "offset $");
		break;
	}
	case output_data_bytes: case output_data_words: case output_reserve_bytes:
		if (ctx->masm && event->kind == output_reserve_bytes) {
			fputs("\tif ", out); write_expr(ctx, first->expression); fputc('\n', out);
		}
		fputs(event->kind == output_data_bytes ? "\tdb " :
		    event->kind == output_data_words ? "\tdw " : ctx->masm ? "\tdb " : "\trb ", out);
		write_expr(ctx, first->expression);
		if (ctx->masm && event->kind == output_reserve_bytes) fputs(" dup(0)\n\tendif", out);
		break;
	case output_origin: {
		char current[80];
		if (ctx->relocatable) {
			unsigned long number = ++ctx->location_count;
			fprintf(out, "p_%lu:\n", number);
			snprintf(current, sizeof(current), "(p_%lu-x_entry+100h)", number);
		} else strcpy(current, ctx->masm ? "($-x_entry+100h)" : "(offset $)");
		/* Compare unsigned offsets without depending on assembler signedness.
		 * RB can silently accept a negative count, so force a diagnostic first. */
		fprintf(out, "\tif (%s shr 1) gt %ld\n", current, first->value >> 1);
		fputs("\tdw x_ovflow ; translated code exceeds ORG\n\tendif\n", out);
		fprintf(out, "\tif (%s shr 1) eq %ld\n\tif (%s and 1) gt %ld\n", current, first->value >> 1, current, first->value & 1);
		fputs("\tdw x_ovflow ; translated code exceeds ORG\n\tendif\n\tendif\n", out);
		if (ctx->masm) fprintf(out, "\tif %ld-%s\n\tdb (%ld-%s) dup(0)\n\tendif", first->value, current, first->value, current);
		else fprintf(out, "\trb (%ld-%s)", first->value, current);
		break;
	}
	case output_alignment:
		if (ctx->relocatable) {
			unsigned long number = ++ctx->location_count;
			fprintf(out, "p_%lu:\n\trb ((0-(p_%lu-x_entry+100h)) and %ld)", number, number, first->value - 1);
		} else if (ctx->masm) fprintf(out, "\tif ((0-($-x_entry+100h)) and %ld)\n\tdb ((0-($-x_entry+100h)) and %ld) dup(0)\n\tendif", first->value - 1, first->value - 1);
		else fprintf(out, "\trb ((0-(offset $)) and %ld)", first->value - 1);
		break;
	case output_comment:
		if (*event->text) fprintf(out, "\t%s%s", *event->text == ';' ? "" : "; ", event->text);
		break;
	case output_line_end: fputc('\n', out); break;
	case output_error_comment: ctx->failed = 1; break;
	}
	ctx->location = NULL;
}

static void finish_equ(asm_context_t *, size_t);

static void
finish_dependencies(asm_context_t *ctx, const expr_t *tree)
{
	const asm_name_t *item;
	if (!tree || ctx->failed) return;
	if (tree->kind == expr_symbol && (item = find_name(ctx, tree->text)) && item->equ)
		finish_equ(ctx, (size_t)(item - ctx->names));
	finish_dependencies(ctx, tree->left); finish_dependencies(ctx, tree->right);
}

static void
finish_equ(asm_context_t *ctx, size_t index)
{
	asm_name_t *item = &ctx->names[index];
	char anchor[32];
	if (item->written == 1) return;
	if (item->written == 2 || ctx->eval_depth >= 128) { ctx->failed = 1; return; }
	item->written = 2;
	++ctx->eval_depth;
	finish_dependencies(ctx, item->equ);
	--ctx->eval_depth;
	snprintf(anchor, sizeof(anchor), "e_%lu", (unsigned long)index);
	ctx->location = anchor;
	fprintf(ctx->out, "%s equ ", item->name);
	write_expr(ctx, item->equ); fputc('\n', ctx->out);
	ctx->location = NULL;
	item->written = 1;
}

int
asmfmt_render(const target_program_t *program, FILE *out, FILE *diagnostics, asm_flavor_t flavor)
{
	int masm = flavor == asm_flavor_masm3;
	const char *name = masm ? "MASM 3" : flavor == asm_flavor_drasm86 ? "RASM-86" : "ASM86";
	asm_context_t ctx = { .out = out, .masm = masm, .relocatable = flavor == asm_flavor_drasm86 };
	emitter_t emitter = { &ctx, write_asm86 };
	size_t i, j;
	ctx.names = calloc(program->count ? program->count : 1, sizeof(*ctx.names));
	if (!ctx.names) return 0;
	for (i = 0; i < program->count; ++i) if (program->events[i].kind == output_label) {
		asm_name_t *item = &ctx.names[ctx.count++];
		item->source = program->events[i].text;
		{
			size_t k, used = 1;
			int safe = strlen(item->source) <= 24;
			item->name[0] = 's';
			for (k = 0; safe && item->source[k]; ++k) {
				unsigned char ch = (unsigned char)item->source[k];
				if (ch == '_') continue; /* ASM86 ignores underscores. */
				if (!isalnum(ch)) safe = 0;
				else item->name[used++] = (char)tolower(ch);
			}
			item->name[used] = 0;
			for (k = 0; safe && k + 1 < ctx.count; ++k)
				if (!strcmp(item->name, ctx.names[k].name)) safe = 0;
			if (!safe) snprintf(item->name, sizeof(item->name), "q%05lu", (unsigned long)ctx.count);
		}
		if (!program->events[i].operands[0].value && i + 1 < program->count)
			item->equ = program->events[i + 1].operands[0].expression;
	}
	for (i = 0; i < program->count; ++i) for (j = 0; j < 2; ++j) {
		const operand_t *op = &program->events[i].operands[j];
		if (op->kind == operand_branch && !op->text) {
			fprintf(diagnostics, "xlt88: %s requires a symbolic branch target\n", name);
			free(ctx.names); return 0;
		}
	}
	if (!target_write(program, &emitter)) ctx.failed = 1;
	for (i = 0; i < ctx.count; ++i) if (ctx.names[i].equ) finish_equ(&ctx, i);
	ctx.memory = 0;
	for (i = 0; i < program->count; ++i) for (j = 0; j < 2; ++j) {
		const operand_t *op = &program->events[i].operands[j];
		if ((op->kind == operand_memory || op->kind == operand_byte_memory) && op->expression) {
			char anchor[32];
			fprintf(out, "m_%lu equ ", ++ctx.memory);
			snprintf(anchor, sizeof(anchor), "a_%lu", ctx.memory);
			ctx.location = anchor;
			write_expr(&ctx, op->expression); fputc('\n', out);
			ctx.location = NULL;
		}
	}
	fputs(masm ? "x_code ends\nend x_entry\n" : ctx.relocatable ? "\tend x_entry\n" : "\tend\n", out);
	free(ctx.names);
	if (ctx.failed) fprintf(diagnostics, "xlt88: %s cannot represent an expression or symbol in this target program\n", name);
	return !ctx.failed && !ferror(out);
}

int
asmfmt_dos(const target_program_t *program, FILE *out, FILE *diagnostics, asm_flavor_t flavor)
{
	FILE *temporary = tmpfile();
	int ch, result;
	if (!temporary) return 0;
	result = asmfmt_render(program, temporary, diagnostics, flavor);
	if (result && !fflush(temporary) && !fseek(temporary, 0, SEEK_SET)) {
		/* CP/M and DOS assembly text uses CRLF records and a Ctrl-Z EOF. */
		while ((ch = fgetc(temporary)) != EOF) {
			if (ch == '\n') fputc('\r', out);
			fputc(ch, out);
		}
		fputc(0x1a, out);
		result = !ferror(temporary) && !ferror(out);
	} else result = 0;
	fclose(temporary);
	return result;
}
