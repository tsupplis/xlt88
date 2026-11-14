#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "dialect.h"

static int
space(char ch)
{
	return ch == ' ' || ch == '\t' || ch == '\f';
}

static void
copy_token(char *dest, const char *begin, const char *end)
{
	while (end > begin && space(end[-1]))
		--end;
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
	    "title subttl page .8080 .z80 .list .xlist macro endm rept irp irpc exitm local "
	    "if ift iff ife if1 if2 ifdef ifndef ifb ifnb ifidn ifdif else endif include .include "
	    "aseg cseg dseg common public extrn entry name set .phase .dephase", word);
}

static const char *
parse_statement(const char *line, statement_t *stmt, int m80)
{
	const char *start, *p = line, *comma = NULL;
	char quote = '\0';
	size_t i;

	memset(stmt, 0, sizeof(*stmt));
	/* A colon makes a label explicit, independent of indentation. M80
	 * additionally accepts known operations at column one. Intel's legacy
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
		while (space(*after)) ++after;
		label = *p == ':' || (start == line && *start && *start != ';' &&
		    (!m80 || !known_operation(token) ||
		    ((!strncmp(after, "equ", 3) || !strncmp(after, "EQU", 3)) &&
		    (space(after[3]) || !after[3]))));
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
		if (in_list("macro endm rept irp irpc exitm local", stmt->opcode))
			return "M80 macro expansion is not implemented";
		if (in_list("if1 if2 ifb ifnb ifidn ifdif", stmt->opcode))
			return "M80 conditional assembly is not implemented";
		if (in_list("aseg cseg dseg common public extrn entry name .phase .dephase", stmt->opcode))
			return "M80 relocation/module directives are not implemented";
		if (!strcmp(stmt->opcode, "set")) return "M80 mutable SET symbols are not implemented";
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
	for (; *p; ++p) {
		if (quote != '\0') {
			if (*p == quote)
				quote = '\0';
		} else if (*p == '\'' || *p == '"') {
			quote = *p;
		} else if (*p == ';') {
			break;
		} else if (*p == '!') {
			return "split instructions at ! before translation";
		} else if (*p == ',' && comma == NULL) {
			comma = p;
		}
	}
	if (quote != '\0')
		return "unterminated quoted operand";

	/* Data declarations pass their complete initializer list to NASM. */
	if (comma != NULL && strcmp(stmt->opcode, "db") &&
	    strcmp(stmt->opcode, "dw")) {
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
		record->problem = problem ? problem : parse_statement(line, &record->statement, m80);
		if (!m80 && !record->problem && !strcmp(record->statement.opcode, "end")) break;
	}
	return !ferror(in);
}

static int
read_intel(FILE *in, source_program_t *program)
{
	return read_source(in, program, 0);
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

const dialect_t intel_dialect = { "intel", read_intel };
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

int
source_read(const dialect_t *dialect, FILE *in, const char *path, source_program_t *program)
{
	return dialect == &m80_dialect ? m80_read_file(in, path, program) : dialect->read(in, program);
}

void
source_report(FILE *out, const source_record_t *record, const char *message)
{
	if (record->origin_file) fprintf(out, "xlt88:%s:%lu: %s\n", record->origin_file, record->origin_line, message);
	else fprintf(out, "xlt88:%lu: %s\n", record->line, message);
}
