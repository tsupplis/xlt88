#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "dialect.h"
#include "expr.h"

/* Until the expression IR has a native word-arithmetic mode, reject
 * operations whose operands need an unrepresented intermediate wrap. */
static int
word_operand(const expr_t *tree)
{
	long value;
	if (expr_eval(tree, NULL, NULL, -1, &value)) return value >= 0 && value <= 65535;
	if (tree->kind == expr_symbol || tree->kind == expr_location) return 1;
	if (tree->kind == expr_binary && tree->op == expr_and) {
		if (expr_eval(tree->left, NULL, NULL, -1, &value) && value >= 0 && value <= 65535) return 1;
		if (expr_eval(tree->right, NULL, NULL, -1, &value) && value >= 0 && value <= 65535) return 1;
	}
	return 0;
}

static int
word_safe(const expr_t *tree)
{
	if (!tree) return 1;
	if (tree->kind == expr_binary && (tree->op == expr_divide || tree->op == expr_modulo ||
	    tree->op == expr_right || tree->op == expr_equal || tree->op == expr_unequal ||
	    tree->op == expr_less || tree->op == expr_lequal || tree->op == expr_greater || tree->op == expr_gequal))
		if (!word_operand(tree->left) || !word_operand(tree->right)) return 0;
	return word_safe(tree->left) && word_safe(tree->right);
}

int
mac_expression(const char *text)
{
	const char *error;
	expr_t *tree;
	int result;
	if (!*text) return 1;
	tree = expr_parse(text, NULL, NULL, 1, &error);
	result = tree && word_safe(tree);
	expr_destroy(tree);
	return result;
}

/* Canonicalise doubled quotes into shared expressions, without relying on
 * assembler-specific escaping. Only escaped literals need rewriting. */
int
mac_literals(char text[256])
{
	char result[256];
	size_t used = 0;
	const char *p = text;
	while (*p) {
		if (*p == '\'' || *p == '"') {
			char quote = *p++, bytes[256], literal[1024];
			const char *start = p - 1;
			size_t count = 0, length;
			int escaped = 0;
			while (*p) {
				if (*p == quote) {
					if (p[1] != quote) break;
					++p; escaped = 1;
				}
				bytes[count++] = *p++;
			}
			if (!*p) return 0;
			++p;
			bytes[count] = 0;
			if (!escaped && quote == '\'') {
				length = (size_t)(p - start);
				memcpy(literal, start, length); literal[length] = 0;
			} else if (count == 1) {
				snprintf(literal, sizeof(literal), "%u", (unsigned char)bytes[0]);
			} else {
				/* ASM86 accepts single-quoted strings only. Apostrophes
				 * become byte items rather than backend escape syntax. */
				length = 0;
				for (size_t i = 0; i < count;) {
					size_t start = i;
					if (length) literal[length++] = ',';
					if (bytes[i] == '\'') {
						literal[length++] = '3'; literal[length++] = '9'; ++i;
					} else {
						literal[length++] = '\'';
						while (i < count && bytes[i] != '\'') ++i;
						memcpy(literal + length, bytes + start, i - start);
						length += i - start;
						literal[length++] = '\'';
					}
				}
				if (!count) { literal[length++] = '\''; literal[length++] = '\''; }
				literal[length] = 0;
			}
			length = strlen(literal);
			if (used + length >= sizeof(result)) return 0;
			memcpy(result + used, literal, length); used += length;
		} else {
			if (used + 1 >= sizeof(result)) return 0;
			result[used++] = *p++;
		}
	}
	result[used] = 0;
	strcpy(text, result);
	return 1;
}

const char *
mac_names(char *text, int generated)
{
	char result[256];
	const char *p;
	size_t used = 0;
	int quote = 0;
	for (p = text; *p;) {
		if (!quote && *p == ';') {
			strcpy(result + used, p); used += strlen(p); break;
		}
		if (*p == '\'' || *p == '"') {
			if (!quote) quote = *p;
			else if (quote == *p) quote = 0;
			result[used++] = *p++;
		} else if (!quote && (isalnum((unsigned char)*p) || *p == '?' || *p == '@' || (generated && *p == '_'))) {
			size_t length = 0;
			int identifier = !isdigit((unsigned char)*p);
			int private = generated && (!strncmp(p, "__m80_", 6) || !strncmp(p, "__mac_", 6));
			if (*p == '_' && !private) return "invalid MAC identifier";
			while (isalnum((unsigned char)*p) || *p == '?' || *p == '@' || *p == '$' || (private && *p == '_')) {
				if (*p != '$') { result[used++] = *p; ++length; }
				++p;
			}
			if (identifier && !private && length > 16) return "MAC identifier exceeds 16 significant characters";
		} else {
			if (!quote && (*p == '_' || (*p == '$' && (isalnum((unsigned char)p[1]) || p[1] == '?' || p[1] == '@'))))
				return "invalid MAC identifier";
			result[used++] = *p++;
		}
	}
	result[used] = 0;
	strcpy(text, result);
	return NULL;
}

/* MAC lexical rules precede the shared macro/conditional engine. */
static const char *
normalize(char text[255])
{
	const char *p = text, *start;
	while (isspace((unsigned char)*p)) ++p;
	start = p;
	while (isdigit((unsigned char)*p)) ++p;
	if (p != start && isspace((unsigned char)*p)) {
		while (isspace((unsigned char)*p)) ++p;
		memmove(text, p, strlen(p) + 1);
	} else if (start != text) memmove(text, start, strlen(start) + 1);
	p = text;
	while (isspace((unsigned char)*p)) ++p;
	if (*p == '$' && (p[1] == '+' || p[1] == '-')) {
		/* Listing-only controls; output-file and library controls are not
		 * silently discarded. Remaining letters spell the control name. */
		int control = toupper((unsigned char)p[2]);
		if (!strchr("MQLI", control) || !control) return "unsupported MAC assembly control";
		for (p += 3; *p && *p != ';'; ++p)
			if (!isalpha((unsigned char)*p) && !isspace((unsigned char)*p)) return "unsupported MAC assembly control";
		strcpy(text, "; MAC listing control");
		return NULL;
	}
	return mac_names(text, 0);
}

static int
append(source_program_t *program, size_t *capacity, const char *text,
    unsigned long line, const char *problem)
{
	source_record_t *record;
	if (program->count >= 65536) return 0;
	if (program->count == *capacity) {
		size_t next = *capacity ? *capacity * 2 : 32;
		void *items = realloc(program->records, next * sizeof(*record));
		if (!items) return 0;
		program->records = items; *capacity = next;
	}
	record = &program->records[program->count++];
	memset(record, 0, sizeof(*record));
	record->line = program->count;
	record->origin_line = line;
	strcpy(record->text, text);
	if (!problem) problem = normalize(record->text);
	record->problem = problem ? problem : m80_statement(record->text, &record->statement, 0);
	return 1;
}

static int
statement(source_program_t *program, size_t *capacity, const char *text,
    unsigned long line, const char *problem)
{
	char label[256], operation[256], operands[256], generated[255];
	source_record_t *record;
	if (!append(program, capacity, text, line, problem)) return 0;
	record = &program->records[program->count - 1];
	if (record->problem || !record->statement.label[0] ||
	    (strcmp(record->statement.opcode, "org") && strcmp(record->statement.opcode, "end") && strcmp(record->statement.opcode, "if"))) return 1;
	strcpy(label, record->statement.label);
	strcpy(operation, record->statement.opcode);
	strcpy(operands, record->statement.first);
	if (record->statement.second[0]) {
		record->problem = "MAC ORG/END accepts one expression";
		return 1;
	}
	/* Keep label values in the shared IR: ORG defines its label after
	 * changing location; END defines its label at the final location. */
	if (!strcmp(operation, "org")) {
		snprintf(generated, sizeof(generated), "\torg %s", operands);
		strcpy(record->text, generated);
		record->problem = m80_statement(record->text, &record->statement, 0);
		snprintf(generated, sizeof(generated), "%s equ $", label);
	} else {
		snprintf(generated, sizeof(generated), "%s equ $", label);
		strcpy(record->text, generated);
		record->problem = m80_statement(record->text, &record->statement, 0);
		snprintf(generated, sizeof(generated), "\t%s %s", operation, operands);
	}
	return append(program, capacity, generated, line, NULL);
}

int
mac_raw_read(FILE *input, source_program_t *program)
{
	char text[255];
	size_t used = 0, capacity = 0;
	unsigned long line = 1;
	int ch, quote = 0, comment = 0;
	const char *problem = NULL;
	while ((ch = fgetc(input)) != EOF && ch != 0x1a) {
		if (ch == '\r' || ch == '\n' || (ch == '!' && !quote && !comment)) {
			text[used] = 0;
			if (!statement(program, &capacity, text, line, problem)) return 0;
			if (ch != '!') {
				++line;
				if (ch == '\r') {
					ch = fgetc(input);
					if (ch != '\n' && ch != EOF) ungetc(ch, input);
				}
			}
			used = 0; quote = comment = 0; problem = NULL;
			continue;
		}
		if (!ch) problem = "NUL byte in MAC source";
		if (!comment) {
			if (ch == '\'' || ch == '"') {
				if (!quote) quote = ch;
				else if (quote == ch) quote = 0;
			} else if (!quote && ch == ';') comment = 1;
		}
		if (used < sizeof(text) - 1) text[used++] = (char)ch;
		else problem = "MAC statement exceeds 254 characters";
	}
	text[used] = 0;
	if ((used || problem) && !statement(program, &capacity, text, line, problem)) return 0;
	return !ferror(input);
}

int
mac_read_file(FILE *input, const char *path, source_program_t *program)
{
	return macro_read_file(input, path, program, mac_raw_read, macro_mac);
}

static int
read_mac(FILE *input, source_program_t *program)
{
	return mac_read_file(input, NULL, program);
}

const dialect_t mac_dialect = { "mac", read_mac };
