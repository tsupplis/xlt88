#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "dialect.h"
#include "expr.h"

static int
space(char ch)
{
	return ch == ' ' || ch == '\t' || ch == '\f';
}

static void
copy_token(char *dest, const char *begin, const char *end)
{
	while (end > begin && space(end[-1])) {
		const char *escape = end - 1;
		while (escape > begin && escape[-1] == '!') --escape;
		if ((end - 1 - escape) % 2) break;
		--end;
	}
	memcpy(dest, begin, end - begin);
	dest[end - begin] = '\0';
}

static int
in_list(const char *list, const char *word)
{
	size_t length = strlen(word);
	while (*list) {
		size_t n = strcspn(list, " ");
		if (n == length && !memcmp(list, word, n)) return 1;
		list += n;
		if (*list) ++list;
	}
	return 0;
}

static int
known_operation(const char *word)
{
	return in_list("nop lxi stax inx inr dcr mvi rlc dad ldax dcx rrc ral rar daa cma stc cmc "
	    "mov hlt add adc sub sbb ana xra ora cmp rnz pop jnz jmp cnz push adi rst rz ret jz cz call aci "
	    "rnc jnc cnc sui rc jc cc sbi rpo jpo cpo ani rpe pchl jpe cpe xri rp jp cp ori rm sphl jm cm cpi "
	    "lda sta lhld shld xthl xchg in out di ei org equ db dw ds align end "
	    "title subttl page .radix .8080 .z80 .list .xlist macro endm rept irp irpc exitm local "
	    "if ift iff ife if1 if2 ifdef ifndef ifb ifnb ifidn ifdif else endif include .include "
	    "aseg cseg dseg common public extrn entry name maclib set .phase .dephase", word);
}

/* Column-one conditional names can also label instructions (IFF DW DOCOL).
 * Keep a syntactically valid condition, including opcode-named symbols, as
 * a directive. Only the otherwise invalid two-operation form is a label. */
static int
conditional_label(const char *word, const char *following, const char *operand)
{
	char expression[256];
	const char *error;
	expr_t *tree;
	int label;
	if (!in_list("if ift ife iff", word) || !known_operation(following)) return 0;
	copy_token(expression, operand, operand + strcspn(operand, ";"));
	tree = expr_parse(expression, NULL, NULL, 0, &error);
	label = tree == NULL;
	expr_destroy(tree);
	return label;
}

static const char *
parse_statement(const char *line, statement_t *stmt, int m80)
{
	const char *start, *p = line, *comma = NULL;
	char quote = '\0';
	size_t i;

	memset(stmt, 0, sizeof(*stmt));
	/* A colon makes a label explicit, independent of indentation. M80
	 * additionally accepts known operations at column one. ASM80's legacy
	 * column-one label convention is otherwise preserved. */
	while (space(*p)) ++p;
	start = p;
	while (*p && !space(*p) && *p != ':' && *p != ';' && *p != '!') ++p;
	{
		char token[256];
		const char *after = p;
		int label;
		copy_token(token, start, p);
		for (i = 0; token[i]; ++i) token[i] = tolower((unsigned char)token[i]);
		if (m80) {
			char *out = token;
			for (i = 0; token[i]; ++i) if (token[i] != '&') *out++ = token[i];
			*out = 0;
		}
		while (space(*after)) ++after;
		{
			char following[256];
			const char *end = after;
			while (*end && !space(*end) && *end != ';') ++end;
			copy_token(following, after, end);
			for (i = 0; following[i]; ++i) following[i] = tolower((unsigned char)following[i]);
			label = *p == ':' || (m80 && in_list("equ macro set", following)) ||
			    (m80 == 1 && start == line && conditional_label(token, following, after)) ||
			    (start == line && *start && *start != ';' &&
			    (!m80 || (m80 != 2 && !known_operation(token))));
		}
		if (label) {
			copy_token(stmt->label, start, p);
			if (*p == ':') ++p;
			if (*p == ':') return "exported labels (::) require relocatable-module support";
		} else p = start;
	}
	while (space(*p))
		++p;
	start = p;
	while (*p && !space(*p) && *p != ';' && *p != '!')
		++p;
	copy_token(stmt->opcode, start, p);
	for (i = 0; stmt->opcode[i]; ++i)
		stmt->opcode[i] = tolower((unsigned char)stmt->opcode[i]);

	if (m80) {
		if (!strcmp(stmt->opcode, ".z80")) return "M80 Z80 mode is unsupported; use 8080 source";
		if (in_list("if1 if2", stmt->opcode))
			return "M80 conditional assembly is not implemented";
		if (m80 != 3 && in_list("aseg cseg dseg common public extrn entry name .phase .dephase", stmt->opcode))
			return "M80 relocation/module directives are not implemented";
		if (in_list("title subttl page .list .xlist .8080", stmt->opcode)) {
			if (stmt->label[0]) return "label on M80 listing or mode directive is unsupported";
			if (in_list(".list .xlist .8080", stmt->opcode)) {
				const char *tail = p;
				while (space(*tail)) ++tail;
				if (*tail && *tail != ';') return "unexpected operand on M80 mode/listing directive";
			}
			/* Keep listing text reviewable without giving it CPU semantics. */
			memset(stmt, 0, sizeof(*stmt));
			stmt->comment[0] = ';';
			strcpy(stmt->comment + 1, line);
			return NULL;
		}
	}

	while (space(*p))
		++p;
	start = p;
	{
		int literal = m80 && (!known_operation(stmt->opcode) ||
		    in_list("macro local irp irpc ifb ifnb ifidn ifdif", stmt->opcode));
		int angle = 0, arg_start = 1;
		for (; *p; ++p) {
			if (quote) { if (*p == quote) quote = 0; }
			else if (literal && *p == '!') {
				if (!p[1]) return "dangling ! in macro argument";
				++p; arg_start = 0;
			} else if (*p == '\'' || *p == '"') { quote = *p; arg_start = 0; }
			else if (literal && *p == '<' && (angle || arg_start)) { ++angle; arg_start = 0; }
			else if (literal && *p == '>' && angle) --angle;
			else if (*p == ';' && !angle) break;
			else if (*p == '!' && !literal) return "split instructions at ! before translation";
			else if (*p == ',' && !angle) {
				if (!comma) comma = p;
				arg_start = 1;
			} else if (!space(*p)) arg_start = 0;
		}
	}
	if (quote != '\0')
		return "unterminated quoted operand";

	/* Data declarations pass their complete initializer list to NASM. */
	if (comma != NULL && strcmp(stmt->opcode, "db") &&
	    strcmp(stmt->opcode, "dw") &&
	    (!m80 || (known_operation(stmt->opcode) &&
	    !in_list("macro local if ift ife iff ifb ifnb ifidn ifdif", stmt->opcode)))) {
		copy_token(stmt->first, start, comma++);
		while (space(*comma))
			++comma;
		copy_token(stmt->second, comma, p);
	} else {
		copy_token(stmt->first, start, p);
	}
	strcpy(stmt->comment, p);
	return NULL;
}

/* Read CP/M text without counting CRLF twice. A record remains one physical
 * line, preserving the source-index/line association used by current passes. */
static int
read_source(FILE *in, source_program_t *program, int m80)
{
	size_t capacity = 0;
	int ended = 0;
	while (!ended) {
		if (program->count >= 65536) return 0;
		char line[255];
		size_t length = 0;
		int ch, present = 0;
		const char *problem = NULL;
		source_record_t *record;
		while ((ch = fgetc(in)) != EOF) {
			if (ch == 0x1a) { ended = 1; break; }
			present = 1;
			if (ch == '\r' || ch == '\n') {
				if (ch == '\r') {
					ch = fgetc(in);
					if (ch != '\n' && ch != EOF) ungetc(ch, in);
				}
				break;
			}
			if (!ch) problem = "NUL byte in assembly source";
			if (length < sizeof(line) - 1) line[length++] = (char)ch;
			else problem = "source line exceeds 254 characters";
		}
		if (!present && !length) break;
		line[length] = 0;
		if (program->count == capacity) {
			size_t next = capacity ? capacity * 2 : 32;
			void *records = realloc(program->records, next * sizeof(*record));
			if (!records) return 0;
			program->records = records;
			capacity = next;
		}
		record = &program->records[program->count];
		memset(record, 0, sizeof(*record));
		record->line = ++program->count;
		record->origin_line = record->line;
		strcpy(record->text, line);
		record->problem = problem ? problem : parse_statement(line, &record->statement, m80);
		if (!m80 && !record->problem && !strcmp(record->statement.opcode, "end")) break;
	}
	return !ferror(in);
}

static int
read_asm80(FILE *in, source_program_t *program)
{
	return read_source(in, program, 0);
}

const char *
m80_statement(const char *text, statement_t *statement, int operation)
{
	return parse_statement(text, statement, operation ? 2 : 1);
}

const char *
rmac_statement(const char *text, statement_t *statement)
{
	return parse_statement(text, statement, 3);
}

int
m80_raw_read(FILE *in, source_program_t *program)
{
	return read_source(in, program, 1);
}

static int
read_m80(FILE *in, source_program_t *program)
{
	return m80_read_file(in, NULL, program);
}

const dialect_t asm80_dialect = { "asm80", read_asm80 };
const dialect_t m80_dialect = { "m80", read_m80 };

void
source_destroy(source_program_t *program)
{
	size_t i;
	for (i = 0; i < program->count; ++i) free(program->records[i].origin_file);
	for (i = 0; i < program->file_count; ++i) free(program->files[i]);
	free(program->files);
	free(program->records);
	memset(program, 0, sizeof(*program));
}

/* Validate CPU operand shapes only after macro expansion. A macro may use an
 * instruction name, and its arguments must not be mistaken for CPU operands.
 * Expression evaluation and target representation stay in their own passes. */
static int
register_in(const char *name, const char *set)
{
	char lower[4];
	size_t length = strlen(name);
	if (!length || length >= sizeof(lower)) return 0;
	for (size_t i = 0; i <= length; ++i) lower[i] = tolower((unsigned char)name[i]);
	return in_list(set, lower);
}

static const char *
instruction_operands(const statement_t *stmt)
{
	const char *op = stmt->opcode, *a = stmt->first, *b = stmt->second;
	const char *bytes = "a b c d e h l m";
	int valid;
	if (in_list("nop rlc rrc ral rar daa cma stc cmc hlt rnz rz ret rnc rc rpo rpe pchl xthl xchg rp rm sphl di ei", op))
		valid = !*a && !*b;
	else if (in_list("inr dcr add adc sub sbb ana xra ora cmp", op))
		valid = register_in(a, bytes) && !*b;
	else if (in_list("inx dcx dad", op))
		valid = register_in(a, "b d h sp") && !*b;
	else if (in_list("push pop", op))
		valid = register_in(a, "b d h psw") && !*b;
	else if (in_list("stax ldax", op))
		valid = register_in(a, "b d") && !*b;
	else if (!strcmp(op, "mov"))
		valid = register_in(a, bytes) && register_in(b, bytes) &&
		    !(register_in(a, "m") && register_in(b, "m"));
	else if (!strcmp(op, "mvi"))
		valid = register_in(a, bytes) && *b;
	else if (!strcmp(op, "lxi"))
		valid = register_in(a, "b d h sp") && *b;
	else if (in_list("lda sta lhld shld jmp jnz jz jnc jc jpo jpe jp jm call cnz cz cnc cc cpo cpe cp cm adi aci sui sbi ani xri ori cpi in out rst", op))
		valid = *a && !*b;
	else return NULL;
	return valid ? NULL : "invalid 8080 instruction operands";
}

int
source_read(const dialect_t *dialect, FILE *in, const char *path, source_program_t *program)
{
	int result;
	if (dialect == &mac_dialect) result = mac_read_file(in, path, program);
	else if (dialect == &rmac_dialect) result = rmac_read_file(in, path, program);
	else result = dialect == &m80_dialect ? m80_read_file(in, path, program) : dialect->read(in, program);
	if (result) {
		for (size_t i = 0; i < program->count; ++i) {
			source_record_t *record = &program->records[i];
			if (!record->problem) record->problem = instruction_operands(&record->statement);
		}
	}
	return result;
}

void
source_report(FILE *out, const source_record_t *record, const char *message)
{
	if (record->origin_file) fprintf(out, "xlt88:%s:%lu: %s\n", record->origin_file, record->origin_line, message);
	else fprintf(out, "xlt88:%lu: %s\n", record->line, message);
}
