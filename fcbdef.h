#ifndef fcbdef_h
#define fcbdef_h

#include "symbols.h"

/* Borrow all inputs. Return 1 for an approved default-FCB plan, 0 when no
 * default FCB is used, or -1 with a static diagnostic. Calls is source-sized.
 * This checks identifiable field accesses, not arbitrary native memory. */
int fcb_default_plan(const source_program_t *, const symbol_table_t *,
    const long *, unsigned char *, const char **);

#endif
