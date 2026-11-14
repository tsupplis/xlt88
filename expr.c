#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "expr.h"

typedef struct expr_parser {
	const char *cursor;
	expr_resolve_t resolve;
	void *context;
	const char *error;
	unsigned depth;
} expr_parser_t;

static char *
copy_span(const char *start, size_t length)
{
	char *text = malloc(length + 1);
	if (text) { memcpy(text, start, length); text[length] = 0; }
	return text;
}

void
expr_destroy(expr_t *tree)
{
	if (!tree) return;
	expr_destroy(tree->left);
	expr_destroy(tree->right);
	free(tree->text);
	free(tree);
}

static expr_t *
node(expr_parser_t *parser, expr_kind_t kind)
{
	expr_t *tree = calloc(1, sizeof(*tree));
	if (tree) tree->kind = kind;
	else parser->error = "cannot allocate expression";
	return tree;
}

static void
skip_space(expr_parser_t *parser)
{
	while (*parser->cursor == ' ' || *parser->cursor == '\t') ++parser->cursor;
}

static int
word(const char *text, const char *name)
{
	size_t i;
	for (i = 0; name[i]; ++i)
		if (tolower((unsigned char)text[i]) != name[i]) return 0;
	return !text[i] || (!isalnum((unsigned char)text[i]) && !strchr("_.$?@#", text[i]));
}

static int
binary_op(expr_parser_t *parser, expr_op_t *op, size_t *length)
{
	const char *p = parser->cursor;
	*length = 1;
	switch (*p) {
	case '|': *op = expr_or; return 1;
	case '^': *op = expr_xor; return 2;
	case '&': *op = expr_and; return 3;
	case '<': if (p[1] == '<') { *op = expr_left; *length = 2; return 4; } break;
	case '>': if (p[1] == '>') { *op = expr_right; *length = 2; return 4; } break;
	case '+': *op = expr_plus; return 5;
	case '-': *op = expr_minus; return 5;
	case '*': *op = expr_multiply; return 6;
	case '/': *op = expr_divide; return 6;
	case '%': *op = expr_modulo; return 6;
	default: break;
	}
	if (word(p, "mod")) { *op = expr_modulo; *length = 3; return 6; }
	if (word(p, "shl") || word(p, "shr")) { *op = word(p, "shl") ? expr_left : expr_right; *length = 3; return 6; }
	if (word(p, "or")) { *op = expr_or; *length = 2; return 1; }
	if (word(p, "xor")) { *op = expr_xor; *length = 3; return 1; }
	{
		static const char *const names[] = {"eq", "ne", "lt", "le", "gt", "ge"};
		size_t i;
		for (i = 0; i < 6; ++i) if (word(p, names[i])) {
			*op = (expr_op_t)(expr_equal + i); *length = 2; return 4;
		}
	}
	if (word(p, "and")) { *op = expr_and; *length = 3; return 3; }
	return 0;
}

static expr_t *parse_binary(expr_parser_t *, int);

/* Lower byte extraction and word NOT into the existing arithmetic IR. */
static expr_t *
with_constant(expr_parser_t *parser, expr_t *left, expr_op_t op, long value, const char *text)
{
	expr_t *tree = node(parser, expr_binary);
	if (!tree) { expr_destroy(left); return NULL; }
	tree->op = op; tree->left = left;
	tree->right = node(parser, expr_integer);
	if (tree->right) {
		tree->right->integer = value;
		tree->right->text = copy_span(text, strlen(text));
		if (!tree->right->text) parser->error = "cannot allocate expression";
	}
	return tree;
}

static expr_t *
parse_atom(expr_parser_t *parser)
{
	const char *start;
	expr_t *tree = NULL;
	size_t length;
	skip_space(parser);
	if (++parser->depth > 128) { parser->error = "expression nesting exceeds 128"; goto done; }
	start = parser->cursor;
	if (word(start, "high") || word(start, "low") || word(start, "not")) {
		int high = word(start, "high"), complement = word(start, "not");
		parser->cursor += high ? 4 : 3;
		tree = complement ? parse_binary(parser, 4) : parse_atom(parser);
		if (!tree) goto done;
		tree = with_constant(parser, tree, complement ? expr_xor : expr_and,
		    complement || high ? 65535 : 255, complement || high ? "65535" : "255");
		if (high && !parser->error) tree = with_constant(parser, tree, expr_right, 8, "8");
		if (complement && !parser->error) tree = with_constant(parser, tree, expr_and, 65535, "65535");
	} else if (*start == '+' || *start == '-' || *start == '~') {
		++parser->cursor;
		tree = node(parser, expr_unary);
		if (!tree) goto done;
		tree->op = *start == '+' ? expr_plus : *start == '-' ? expr_minus : expr_complement;
		tree->left = parse_atom(parser);
	} else if (*start == '(') {
		++parser->cursor;
		tree = parse_binary(parser, 1);
		skip_space(parser);
		if (*parser->cursor != ')') parser->error = "missing closing parenthesis";
		else ++parser->cursor;
	} else if (*start == '\'' || *start == '"') {
		++parser->cursor;
		while (*parser->cursor && *parser->cursor != *start) ++parser->cursor;
		if (!*parser->cursor) { parser->error = "unterminated expression string"; goto done; }
		++parser->cursor;
		tree = node(parser, expr_string);
		if (tree) tree->text = copy_span(start, parser->cursor - start);
	} else if (*start && (isalnum((unsigned char)*start) || strchr("_.$?@#", *start))) {
		char *raw;
		while (*parser->cursor && (isalnum((unsigned char)*parser->cursor) ||
		    strchr("_.$?@#", *parser->cursor))) ++parser->cursor;
		length = parser->cursor - start;
		raw = copy_span(start, length);
		if (!raw) { parser->error = "cannot allocate expression"; goto done; }
		tree = node(parser, expr_symbol);
		if (!tree) { free(raw); goto done; }
		tree->text = raw;
		if (!strcmp(raw, "$")) tree->kind = expr_location;
		else if (isdigit((unsigned char)*raw)) {
			char *end;
			int base = 10, suffix = 0;
			if (tolower((unsigned char)raw[length - 1]) == 'h') { base = 16; suffix = 1; }
			else if (length > 2 && raw[0] == '0' && tolower((unsigned char)raw[1]) == 'x') base = 16;
			else if (tolower((unsigned char)raw[length - 1]) == 'b') { base = 2; suffix = 1; }
			else if (tolower((unsigned char)raw[length - 1]) == 'd') { base = 10; suffix = 1; }
			else if (tolower((unsigned char)raw[length - 1]) == 'o' || tolower((unsigned char)raw[length - 1]) == 'q') { base = 8; suffix = 1; }
			errno = 0;
			tree->integer = strtol(raw, &end, base);
			if (errno || end != raw + length - suffix) parser->error = "invalid or overflowing integer";
			tree->kind = expr_integer;
		} else if (parser->resolve) {
			const char *name = parser->resolve(parser->context, raw, &tree->is_equ, &tree->symbol_id);
			if (name) {
				tree->text = copy_span(name, strlen(name));
				free(raw);
			} else parser->error = "undefined expression symbol";
		}
	} else parser->error = "expected expression operand";
	if (tree && (tree->kind == expr_integer || tree->kind == expr_string || tree->kind == expr_symbol || tree->kind == expr_location) && !tree->text)
		parser->error = "cannot allocate expression";
done:
	--parser->depth;
	if (parser->error) { expr_destroy(tree); return NULL; }
	return tree;
}

static expr_t *
parse_binary(expr_parser_t *parser, int minimum)
{
	expr_t *left = parse_atom(parser);
	while (left && !parser->error) {
		expr_t *tree;
		expr_op_t op;
		size_t length;
		int precedence;
		skip_space(parser);
		precedence = binary_op(parser, &op, &length);
		if (precedence < minimum) break;
		parser->cursor += length;
		tree = node(parser, expr_binary);
		if (!tree) { expr_destroy(left); return NULL; }
		tree->op = op;
		tree->left = left;
		tree->right = parse_binary(parser, precedence + 1);
		left = tree;
	}
	if (parser->error) { expr_destroy(left); return NULL; }
	return left;
}

static int
valid_arithmetic(const expr_t *tree)
{
	long a, b, value;
	if (!tree) return 1;
	if (!valid_arithmetic(tree->left) || !valid_arithmetic(tree->right)) return 0;
	if (tree->kind == expr_binary && expr_eval(tree->left, NULL, NULL, -1, &a) &&
	    expr_eval(tree->right, NULL, NULL, -1, &b))
		return expr_eval(tree, NULL, NULL, -1, &value);
	if (tree->kind == expr_unary && expr_eval(tree->left, NULL, NULL, -1, &a))
		return expr_eval(tree, NULL, NULL, -1, &value);
	return 1;
}

expr_t *
expr_parse(const char *text, expr_resolve_t resolve, void *context, int list,
    const char **error)
{
	expr_parser_t parser = { .cursor = text, .resolve = resolve, .context = context };
	expr_t *tree = parse_binary(&parser, 1);
	while (tree && list && *parser.cursor == ',') {
		expr_t *next = node(&parser, expr_list);
		if (!next) break;
		++parser.cursor;
		next->left = tree;
		next->right = parse_binary(&parser, 1);
		tree = next;
	}
	skip_space(&parser);
	if (!parser.error && *parser.cursor) parser.error = "unsupported expression operator or trailing text";
	if (!parser.error && !valid_arithmetic(tree)) parser.error = "invalid constant arithmetic or overflow";
	if (parser.error) { expr_destroy(tree); tree = NULL; }
	if (error) *error = parser.error;
	return tree;
}

int
expr_eval(const expr_t *tree, expr_lookup_t lookup, void *context, long location, long *value)
{
	long a, b;
	if (!tree) return 0;
	switch (tree->kind) {
	case expr_integer: *value = tree->integer; return 1;
	case expr_location: if (location < 0) return 0; *value = location; return 1;
	case expr_symbol: return lookup && lookup(context, tree->symbol_id, tree->text, value);
	case expr_string:
		if (strlen(tree->text) != 3) return 0;
		*value = (unsigned char)tree->text[1]; return 1;
	case expr_list: return 0;
	case expr_unary:
		if (!expr_eval(tree->left, lookup, context, location, &a)) return 0;
		if (tree->op == expr_minus && a == LONG_MIN) return 0;
		*value = tree->op == expr_minus ? -a : tree->op == expr_complement ? ~a : a;
		return 1;
	case expr_binary: break;
	}
	if (!expr_eval(tree->left, lookup, context, location, &a) ||
	    !expr_eval(tree->right, lookup, context, location, &b)) return 0;
	switch (tree->op) {
	case expr_plus:
		if ((b > 0 && a > LONG_MAX - b) || (b < 0 && a < LONG_MIN - b)) return 0;
		*value = a + b; return 1;
	case expr_minus:
		if ((b < 0 && a > LONG_MAX + b) || (b > 0 && a < LONG_MIN + b)) return 0;
		*value = a - b; return 1;
	case expr_multiply:
		if (a > 0 ? (b > 0 ? a > LONG_MAX / b : b < LONG_MIN / a) :
		    (a < 0 && (b > 0 ? a < LONG_MIN / b : b < 0 && a < LONG_MAX / b))) return 0;
		*value = a * b; return 1;
	case expr_divide: case expr_modulo:
		if (!b || (a == LONG_MIN && b == -1)) return 0;
		*value = tree->op == expr_divide ? a / b : a % b; return 1;
	case expr_and: *value = a & b; return 1;
	case expr_or: *value = a | b; return 1;
	case expr_xor: *value = a ^ b; return 1;
	case expr_left: case expr_right:
		if (a < 0 || b < 0 || b >= (long)(sizeof(long) * CHAR_BIT - 1)) return 0;
		if (tree->op == expr_left && a > (LONG_MAX >> b)) return 0;
		*value = tree->op == expr_left ? a << b : a >> b; return 1;
	case expr_equal: *value = a == b ? 65535 : 0; return 1;
	case expr_unequal: *value = a != b ? 65535 : 0; return 1;
	case expr_less: *value = a < b ? 65535 : 0; return 1;
	case expr_lequal: *value = a <= b ? 65535 : 0; return 1;
	case expr_greater: *value = a > b ? 65535 : 0; return 1;
	case expr_gequal: *value = a >= b ? 65535 : 0; return 1;
	case expr_complement: return 0;
	}
	return 0;
}
