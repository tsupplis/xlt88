#ifndef recognition_h
#define recognition_h

#include "dialect.h"

/* Record input-C facts (proven values or guarded candidates) per instruction.
 * Recognition does not select an OS or alter the source instructions. */
typedef long (*resolve_value_t)(void *, const char *);
#define os_value_limit 4
typedef struct os_values {
	unsigned char count; /* Zero means unknown, never the empty set. */
	unsigned char guarded; /* Memory-derived candidates need a runtime check. */
	unsigned char values[os_value_limit];
} os_values_t;
/* On success, caller owns one fact per source record and releases it with free.
 * The resolver and its context are borrowed; negative results mean unknown.
 * Failure returns zero with a NULL output. Neither input is modified. */
int recognize_os_sets(const source_program_t *, resolve_value_t, void *, os_values_t **);
/* Project a proven singleton; unknown, multiple or guarded facts return -1. */
int recognize_os_calls(const source_program_t *, resolve_value_t, void *, long **);

#endif
