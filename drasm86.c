#include "drasm86.h"
#include "asmfmt.h"

int
drasm86_render(const target_program_t *program, FILE *out, FILE *diagnostics)
{
	return asmfmt_dos(program, out, diagnostics, asm_flavor_drasm86);
}
