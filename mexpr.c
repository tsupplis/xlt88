#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "mexpr.h"

static int
word(int ch)
{
	return ch && (isalnum((unsigned char)ch) || strchr("_.$?@#", ch));
}

void
mexpr_number(char out[32], unsigned long value, unsigned radix)
{
	static const char digits[] = "0123456789ABCDEF";
	char reversed[32];
	size_t n = 0, used = 0;
	value &= 65535UL;
	do { reversed[n++] = digits[value % radix]; value /= radix; } while (value);
	if (isalpha((unsigned char)reversed[n - 1])) out[used++] = '0';
	while (n) out[used++] = reversed[--n];
	out[used] = 0;
}

const char *
mexpr_text(char text[256], unsigned radix)
{
	char out[256];
	const char *p = text;
	size_t used = 0;
	int quote = 0;
	if (radix < 2 || radix > 16) return "invalid expression radix";
	while (*p) {
		char token[256], number[32];
		const char *replacement = p;
		size_t length = 1, output;
		if (*p == '\'' || *p == '"') {
			if (!quote) quote = *p;
			else if (*p == quote) quote = 0;
		} else if (!quote && word(*p)) {
			size_t i;
			length = 0;
			while (word(p[length])) { token[length] = p[length]; ++length; }
			token[length] = 0;
			if (length == 3 && tolower((unsigned char)token[0]) == 'n' &&
			    tolower((unsigned char)token[1]) == 'u' && tolower((unsigned char)token[2]) == 'l') {
				const char *tail = p + length;
				while (*tail == ' ' || *tail == '\t') ++tail;
				replacement = *tail ? "0D" : "65535D";
				length = strlen(p); /* NUL consumes all remaining argument text. */
			} else if (isdigit((unsigned char)token[0])) {
				unsigned base = radix;
				size_t digits = length, start = 0;
				unsigned long value = 0;
				int suffix = tolower((unsigned char)token[length - 1]);
				if (length > 2 && token[0] == '0' && tolower((unsigned char)token[1]) == 'x') { base = 16; start = 2; }
				else if (suffix == 'h') { base = 16; --digits; }
				else if (suffix == 'b') { base = 2; --digits; }
				else if (suffix == 'd') { base = 10; --digits; }
				else if (suffix == 'o' || suffix == 'q') { base = 8; --digits; }
				if (digits == start) return "numeric literal has no digits";
				for (i = start; i < digits; ++i) {
					int ch = tolower((unsigned char)token[i]);
					unsigned digit = ch >= '0' && ch <= '9' ? (unsigned)(ch - '0') :
					    ch >= 'a' && ch <= 'f' ? (unsigned)(ch - 'a' + 10) : 99;
					if (digit >= base) return "digit is invalid in the active radix";
					if (value > ((unsigned long)LONG_MAX - digit) / base) return "numeric literal overflows checked arithmetic";
					value = value * base + digit;
				}
				/* Explicit source suffixes keep their established spelling. */
				if (radix != 10 && base == radix && digits == length && !start) {
					snprintf(number, sizeof(number), "%luD", value);
					replacement = number;
				}
			}
		}
		output = replacement == p ? length : strlen(replacement);
		if (used + output >= sizeof(out)) return "normalized expression exceeds operand size limit";
		memcpy(out + used, replacement, output); used += output; p += length;
	}
	out[used] = 0;
	strcpy(text, out);
	return NULL;
}
