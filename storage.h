#ifndef storage_h
#define storage_h
#include "symbols.h"

/* Find a DS reservation at a symbol, across only blank/comment records.
 * The caller owns alias and use analysis; this proves declaration extent only. */
int storage_find(const source_program_t *, const symbol_table_t *,
    const symbol_t *, size_t *, long *);
#endif
