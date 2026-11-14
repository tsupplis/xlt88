#ifndef asmfmt_h
#define asmfmt_h
#include "target.h"
typedef enum asm_flavor {
	asm_flavor_asm86, asm_flavor_masm3, asm_flavor_drasm86
} asm_flavor_t;
int asmfmt_render(const target_program_t *, FILE *, FILE *, asm_flavor_t);
int asmfmt_dos(const target_program_t *, FILE *, FILE *, asm_flavor_t);
#endif
