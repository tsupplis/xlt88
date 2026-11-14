#ifndef recognition_h
#define recognition_h

#include "dialect.h"

/* Recognition records proven input-C values at each source instruction.
 * It does not select an OS or alter the source instructions. */
typedef long (*resolve_value_t)(void *, const char *);
#define os_value_limit 4
typedef struct os_values {
	unsigned char count; /* Zero means unknown, never the empty set. */
	unsigned char values[os_value_limit];
} os_values_t;
/* On success, caller owns one fact per source record and releases it with free.
 * The resolver and its context are borrowed; negative results mean unknown.
 * Failure returns zero with a NULL output. Neither input is modified. */
int recognize_os_sets(const source_program_t *, resolve_value_t, void *, os_values_t **);
/* Compatibility projection: a singleton value, or -1 for unknown/multiple. */
int recognize_os_calls(const source_program_t *, resolve_value_t, void *, long **);

#endif
