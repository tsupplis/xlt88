#ifndef regions_h
#define regions_h

#include "symbols.h"

/* Owns an emission order of original record indices. Source IDs and addresses
 * are unchanged. Fixed regions may be sorted, never relocated or overlaid. */
typedef struct region_plan {
	size_t *order;
	size_t count;
	int reordered;
} region_plan_t;

int regions_build(const source_program_t *, const symbol_table_t *, region_plan_t *, FILE *);
void regions_destroy(region_plan_t *);

#endif
