#ifndef translator_h
#define translator_h

#include "backend.h"
#include "dialect.h"
#include "target.h"
#include "symbols.h"

/* Source is borrowed and unchanged. Output must be zero-initialized.
 * Returns zero on success; destroy output even after a failure. */
int lower_8080(const source_program_t *, const backend_t *,
    target_program_t *, FILE *diagnostics);

/* The caller owns and reconciles the shared symbol table. */
int lower_named(const source_program_t *, const symbol_table_t *,
    const backend_t *, target_program_t *, FILE *);

#endif
