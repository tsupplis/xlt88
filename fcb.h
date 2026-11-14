#ifndef fcb_h
#define fcb_h

#include "symbols.h"

typedef struct fcb_plan {
	/* One entry per source record; nonzero only after a whole-program audit. */
	unsigned char *calls;
	unsigned char *grow;
} fcb_plan_t;

int fcb_analyze(const source_program_t *, const symbol_table_t *,
    const long *, fcb_plan_t *, FILE *);
void fcb_destroy(fcb_plan_t *);

#endif
