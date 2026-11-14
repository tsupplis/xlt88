#include "masm.h"
#include "asmfmt.h"

int
masm_render(const target_program_t *program, FILE *out, FILE *diagnostics)
{
	FILE *temporary = tmpfile();
	int ch, result;
	if (!temporary) return 0;
	result = asmfmt_render(program, temporary, diagnostics, 1);
	if (result && !fflush(temporary) && !fseek(temporary, 0, SEEK_SET)) {
		/* MASM 3 treats bare LF as whitespace rather than a line ending. */
		while ((ch = fgetc(temporary)) != EOF) {
			if (ch == '\n') fputc('\r', out);
			fputc(ch, out);
		}
		result = !ferror(temporary) && !ferror(out);
	} else result = 0;
	fclose(temporary);
	return result;
}
