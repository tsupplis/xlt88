#ifndef asm86_h
#define asm86_h

#include "target.h"

/* Borrow an immutable target; return nonzero on successful rendering. */
int asm86_render(const target_program_t *, FILE *, FILE *);

#endif
