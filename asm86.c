#include "asm86.h"
#include "asmfmt.h"

int
asm86_render(const target_program_t *program, FILE *out, FILE *diagnostics)
{
	return asmfmt_render(program, out, diagnostics, asm_flavor_asm86);
}
