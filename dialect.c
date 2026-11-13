#include <ctype.h>
#include <string.h>

#include "dialect.h"

static int
space(char ch)
{
	return ch == ' ' || ch == '\t';
}

static void
copy_token(char *dest, const char *begin, const char *end)
{
	while (end > begin && space(end[-1]))
		--end;
	memcpy(dest, begin, end - begin);
	dest[end - begin] = '\0';
}

static const char *
parse_intel(const char *line, struct statement *stmt)
{
	const char *start, *p = line, *comma = NULL;
	char quote = '\0';
	size_t i;

	memset(stmt, 0, sizeof(*stmt));
	if (*p != '\0' && !space(*p) && *p != ';') {
		start = p;
		while (*p && !space(*p) && *p != ':' && *p != ';' && *p != '!')
			++p;
		copy_token(stmt->label, start, p);
		if (*p == ':')
			++p;
	}
	while (space(*p))
		++p;
	start = p;
	while (*p && !space(*p) && *p != ';' && *p != '!')
		++p;
	copy_token(stmt->opcode, start, p);
	for (i = 0; stmt->opcode[i]; ++i)
		stmt->opcode[i] = tolower((unsigned char)stmt->opcode[i]);

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

const struct dialect intel_dialect = { "intel", parse_intel };
