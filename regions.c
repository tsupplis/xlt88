#include <stdlib.h>
#include <string.h>
#include "regions.h"

typedef struct source_region {
	size_t first, last;
	long origin, limit;
} source_region_t;

void
regions_destroy(region_plan_t *plan)
{
	free(plan->order);
	memset(plan, 0, sizeof(*plan));
}

static int
compare(const void *left, const void *right)
{
	const source_region_t *a = left, *b = right;
	if (a->origin != b->origin) return a->origin < b->origin ? -1 : 1;
	return a->first < b->first ? -1 : a->first != b->first;
}

int
regions_build(const source_program_t *source, const symbol_table_t *symbols,
    region_plan_t *plan, FILE *diagnostics)
{
	source_region_t *regions;
	size_t i, j, count = 1, stop = source->count;
	int reorder = 0, known = 1;
	long previous = 0x100, high = 0x100;
	const char *problem = "cannot allocate source regions";
	regions = calloc(source->count + 1, sizeof(*regions));
	plan->order = malloc((source->count ? source->count : 1) * sizeof(*plan->order));
	if (!regions || !plan->order) goto failure;
	regions[0].origin = regions[0].limit = 0x100;
	for (i = 0; i < source->count; ++i) {
		const statement_t *stmt = &source->records[i].statement;
		long origin, size, pc = symbols->addresses[i];
		if (!strcmp(stmt->opcode, "end")) {
			problem = "END entry must resolve to 100h for a native transient";
			if (stmt->second[0] || (stmt->first[0] &&
			    (!symbols_eval(symbols, stmt->first, pc, &origin) || origin != 0x100))) goto record_failure;
			stop = i; break;
		}
		if (!strcmp(stmt->opcode, "org")) {
			problem = "ORG needs a known unlabelled 16-bit source address";
			if (stmt->label[0] || stmt->second[0] ||
			    !symbols_eval(symbols, stmt->first, pc, &origin) || origin < 0x100 || origin > 65535) goto record_failure;
			if (origin < previous) reorder = 1;
			previous = origin;
			regions[count - 1].last = i;
			regions[count].first = i;
			regions[count].origin = regions[count].limit = origin;
			++count;
		} else {
			size = source_size(stmt, symbols, pc);
			if (pc < 0 || size < 0 || size > 65536 - pc) known = 0;
			else if (pc + size > regions[count - 1].limit) regions[count - 1].limit = pc + size;
		}
	}
	regions[count - 1].last = stop;
	if (!reorder) {
		/* Preserve existing handling of unknown sizes in ordinary streams. */
		for (i = 0; i < stop; ++i) plan->order[plan->count++] = i;
		free(regions);
		return 1;
	}
	problem = "out-of-order regions require known source sizes";
	if (!known) goto failure;
	plan->reordered = 1;
	qsort(regions, count, sizeof(*regions), compare);
	for (i = 0; i < count; ++i) {
		problem = "overlapping source regions cannot be reordered";
		if (regions[i].origin < high) goto failure;
		if (regions[i].limit > high) high = regions[i].limit;
		for (j = regions[i].first; j < regions[i].last; ++j)
			plan->order[plan->count++] = j;
	}
	free(regions);
	return 1;
record_failure:
	source_report(diagnostics, &source->records[i], problem);
	free(regions);
	regions_destroy(plan);
	return 0;
failure:
	fprintf(diagnostics, "xlt88: %s\n", problem);
	free(regions);
	regions_destroy(plan);
	return 0;
}
