#include "backend.h"

static void
exit_program(FILE *out)
{
	/* DOS 1.0 loads a COM file with CS pointing to its PSP. */
	fputs("int 20h ; terminate DOS COM program", out);
}

static int
bdos(FILE *out, long function)
{
	switch (function) {
	case 1:
		fputs("mov ah, 01h ; DOS console input with echo\n", out);
		break;
	case 2:
		fputs("mov ah, 02h ; DOS console output, character in DL\n", out);
		break;
	case 9:
		fputs("mov ah, 09h ; DOS print dollar-terminated string at DS:DX\n", out);
		break;
	default:
		return 0;
	}
	fputs("\tint 21h", out);
	return 1;
}

const struct backend msdos_backend = { "msdos", bdos, exit_program, NULL };
