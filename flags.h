#ifndef flags_h
#define flags_h

#include "dialect.h"

/* True only when a straight-line successor overwrites every 8080 flag
 * before any possible observation. Borrows immutable source records. */
int flags_dead_after(const source_program_t *, size_t);

#endif
