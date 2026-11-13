#include "backend.h"

static void
exit_program(FILE *out)
{
	fputs("mov cl, 0 ; CP/M-86 terminate program\n"
	    "\tmov dl, 0\n\tint 0E0h", out);
}

static int
bdos(FILE *out, long function)
{
	const char *service;

	switch (function) {
	case 1:
		service = "console input with echo";
		break;
	case 2:
		service = "console output, character in DL";
		break;
	case 9:
		service = "print dollar-terminated string at DS:DX";
		break;
	default:
		return 0;
	}

	/* These services have no ES result. Keep the caller's segment. */
	fprintf(out, "push es ; CP/M-86 BDOS %ld, %s\n"
	    "\tint 0E0h\n\tpop es", function, service);
	return 1;
}

static void
stack_segment(FILE *out)
{
	/* The loader can leave SS pointing into the CCP. An 8080 program
	 * that chooses SP expects its stack in the data segment. MOV SS
	 * defers interrupts through the following MOV SP. */
	fputs("mov si, ds ; select program stack segment\n\tmov ss, si\n\t", out);
}

const struct backend cpm86_backend = { "cpm86", bdos, exit_program, stack_segment };
