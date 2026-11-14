#include <string.h>

#include "emitter.h"

static void
write_expr(FILE *out, const expr_t *tree)
{
	static const char *const operators[] = { "+", "-", "*", "/", "%", "&", "|", "^", "<<", ">>", "~", "=", "!=", "<", "<=", ">", ">=" };
	switch (tree->kind) {
	case expr_integer: case expr_string: fputs(tree->text, out); break;
	case expr_location: fputs("($ - $$ + 100h)", out); break;
	case expr_symbol:
		if (tree->is_equ) fprintf(out, "$%s", tree->text);
		else fprintf(out, "($%s - $$ + 100h)", tree->text);
		break;
	case expr_unary:
		fprintf(out, "(%s", operators[tree->op]);
		write_expr(out, tree->left);
		fputc(')', out);
		break;
	case expr_binary:
		if (tree->op >= expr_equal) fputs("((-", out);
		fputc('(', out);
		write_expr(out, tree->left);
		fprintf(out, " %s ", operators[tree->op]);
		write_expr(out, tree->right);
		fputc(')', out);
		if (tree->op >= expr_equal) fputs(") & 65535)", out);
		break;
	case expr_list:
		write_expr(out, tree->left);
		fputc(',', out);
		write_expr(out, tree->right);
		break;
	}
}

static void
expression(FILE *out, const operand_t *operand)
{
	write_expr(out, operand->expression);
}

static void
internal_label(FILE *out, const char *prefix, long number)
{
	fprintf(out, "L@%s%ld", prefix ? prefix : "", number);
}

static void
write_operand(FILE *out, const operand_t *value)
{
	switch (value->kind) {
	case operand_register: fputs(value->text, out); break;
	case operand_immediate: fprintf(out, value->value >= 0xa0 ? "0%02lxh" : "%02lxh", value->value); break;
	case operand_interrupt: fprintf(out, value->value >= 0xa0 ? "0%02lxh" : "%02lxh", value->value); break;
	case operand_expression: expression(out, value); break;
	case operand_branch:
		if (value->text)
			fprintf(out, "$%s", value->text);
		else
			fprintf(out, "0x%lx", value->value);
		break;
	case operand_byte_memory:
	case operand_memory:
		fputs(value->kind == operand_byte_memory ? "byte [" : "[", out);
		if (value->expression)
			expression(out, value);
		else
			fputs(value->text, out);
		fputc(']', out);
		break;
	case operand_internal_label: break;
	}
}

static void
write_nasm(void *context, const output_t *event)
{
	FILE *out = context;
	const operand_t *first = &event->operands[0];
	const operand_t *second = &event->operands[1];

	switch (event->kind) {
	case output_header:
		fprintf(out, "bits 16\ncpu 8086\norg 100h\n; Target: %s\n", event->text);
		break;
	case output_instruction:
		fprintf(out, "\t%s", event->text);
		if (first->kind == operand_internal_label) {
			fputc(' ', out);
			internal_label(out, first->text, second->value);
		} else if (first->kind != operand_register || first->text) {
			fputc(' ', out);
			write_operand(out, first);
			if (second->kind != operand_register || second->text) {
				fputs(", ", out);
				write_operand(out, second);
			}
		}
		break;
	case output_label:
		fprintf(out, "$%s%s", event->text, first->value ? ":" : "");
		break;
	case output_internal_definition:
		internal_label(out, event->text, first->value);
		fputs(":\n", out);
		break;
	case output_equate:
	case output_data_bytes:
	case output_data_words:
	case output_reserve_bytes:
		fputs(event->kind == output_equate ? "\tequ " :
		    event->kind == output_data_bytes ? "\tdb " :
		    event->kind == output_data_words ? "\tdw " : "\ttimes (", out);
		expression(out, first);
		if (event->kind == output_reserve_bytes)
			fputs(") db 0", out);
		break;
	case output_origin:
		fprintf(out, "\ttimes (%ld-($-$$+100h)) db 0", first->value);
		break;
	case output_alignment: fprintf(out, "\talign %ld, db 0", first->value); break;
	case output_comment:
		if (*event->text)
			fprintf(out, "\t%s%s", *event->text == ';' ? "" : "; ", event->text);
		break;
	case output_line_end: fputc('\n', out); break;
	case output_error_comment:
		fprintf(out, "; xlt88 error on source line %lu\n", (unsigned long)first->value);
		break;
	}
}

const emitter_t nasm_emitter = { NULL, write_nasm };
