#ifndef flags_h
#define flags_h

#include "dialect.h"

enum {
	flag_carry = 1, flag_auxiliary = 2, flag_zero = 4,
	flag_sign = 8, flag_parity = 16, flag_all = 31
};

/* Return an owned array of live-after masks, indexed by original source
 * record. A clear bit proves that flag unobservable before replacement.
 * The source is borrowed and unchanged. Unknown edges and returns retain
 * all flags in shared bodies. Stack-neutral, store-free leaf summaries may
 * refine incoming flags at an individual direct call using its continuation.
 * Nested calls and unresolved paths retain the conservative plan.
 * On allocation failure, return zero and set output to NULL.
 * The caller releases the array with free(). No OS/emitter dependency. */
int flags_live_plan(const source_program_t *, unsigned char **);

#endif
