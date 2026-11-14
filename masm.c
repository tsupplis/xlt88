#include "masm.h"
#include "asmfmt.h"

int
masm_render(const target_program_t *program, FILE *out, FILE *diagnostics)
{
	return asmfmt_dos(program, out, diagnostics, asm_flavor_masm3);
}
