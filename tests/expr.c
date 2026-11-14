#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "expr.h"

static void
check(const char *text, long expected)
{
	const char *error;
	long value;
	expr_t *tree = expr_parse(text, NULL, NULL, 0, &error);
	assert(tree && !error);
	assert(expr_eval(tree, NULL, NULL, -1, &value));
	assert(value == expected);
	expr_destroy(tree);
}

int
main(void)
{
	const char *error;
	const char *bad[] = {"", "1+", "1/0", "1%0", "1<<99", "(-1)<<2", "1 2", "(1+2", "1+2)", "10h AND", "1,2"};
	char text[128];
	size_t i;
	check("2+3*4", 14);
	check("(2+3)*4", 20);
	check("20/2/2", 5);
	check("9-4-2", 3);
	check("(1234h / 100h) AND 0ffh", 0x12);
	check("101b+17q+10d+0x10", 46);
	check("'A'+1", 66);
	check("-7*3", -21);
	check("-7*-3", 21);
	check("7*-3", -21);
	check("7%3", 1);
	check("1<<4+1", 32);
	check("~0", -1);
	check("1234h SHR 8 + 128", 146);
	check("1 SHL 3 + 1", 9);
	check("10 MOD 3", 1);
	check("1 OR 3 XOR 2", 1);
	check("HIGH 1234h", 18);
	check("LOW 1234h", 52);
	check("HIGH -1", 255);
	check("NOT 0", 65535);
	check("NOT 1 EQ 1", 0);
	check("1 EQ 1", 65535);
	check("1 NE 1", 0);
	check("1 LT 2", 65535);
	check("1 LE 1", 65535);
	check("2 GT 1", 65535);
	check("2 GE 3", 0);
	for (i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
		expr_t *tree = expr_parse(bad[i], NULL, NULL, 0, &error);
		assert(!tree && error);
	}
	snprintf(text, sizeof(text), "%ld+1", LONG_MAX);
	assert(!expr_parse(text, NULL, NULL, 0, &error));
	snprintf(text, sizeof(text), "%ld*2", LONG_MAX);
	assert(!expr_parse(text, NULL, NULL, 0, &error));
	snprintf(text, sizeof(text), "(-%ld-1)/-1", LONG_MAX);
	assert(!expr_parse(text, NULL, NULL, 0, &error));
	puts("Expression precedence, literals and checked arithmetic passed");
	return 0;
}
