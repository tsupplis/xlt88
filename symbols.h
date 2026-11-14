#ifndef symbols_h
#define symbols_h

#include "dialect.h"
#include "expr.h"
#include <stdint.h>

typedef size_t symbol_id_t;
typedef struct symbol {
	symbol_id_t id;
	char name[256];
	char preferred[256];
	char value[256];
	int is_equ;
	unsigned long line;
	long source_address;
	long definition_address;
	expr_t *expression;
} symbol_t;

typedef struct symbol_table {
	symbol_t *items;
	size_t count;
	uint64_t fingerprint;
	long *addresses; /* Original address at each source record, -1 if unknown. */
	size_t address_count;
} symbol_table_t;

/* Tables own their entries; source and diagnostic streams are borrowed.
 * All functions returning int use nonzero for success. IDs are stable for
 * an unchanged parsed source and are scoped to its fingerprint. */
int symbols_build(const source_program_t *, symbol_table_t *, FILE *);
void symbols_destroy(symbol_table_t *);
const symbol_t *symbol_find(const symbol_table_t *, const char *);
const char *symbol_alias(const symbol_table_t *, const char *);
long symbol_number(const char *);
long symbol_value(const symbol_table_t *, const char *);
int symbol_equal(const char *, const char *);
long source_size(const statement_t *, const symbol_table_t *, long);
int symbols_layout(const source_program_t *, symbol_table_t *, FILE *);
int symbols_eval(const symbol_table_t *, const char *, long, long *);
int symbols_read(symbol_table_t *, FILE *, FILE *);
int symbols_write(const symbol_table_t *, FILE *);
int symbols_validate(const symbol_table_t *, FILE *);
/* Parse and render a normalized expression using preferred symbol names. */
int symbols_text(const symbol_table_t *, const char *, char *, size_t);
int emit_named_8080(const source_program_t *, const symbol_table_t *, FILE *, FILE *);

#endif
