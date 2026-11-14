#ifndef macro_h
#define macro_h
#include "dialect.h"

typedef int (*macro_eval_t)(void *, const char *, long *);

typedef struct macro {
	char name[256];
	char parameters[32][256];
	char locals[32][256];
	size_t parameter_count, local_count;
	source_program_t body;
	struct macro *next;
} macro_t;

macro_t *macro_find(macro_t *, const char *);
const char *macro_define(macro_t **, const source_program_t *, size_t, size_t, const char *);
const char *macro_expand(const macro_t *, const statement_t *, macro_eval_t, void *, unsigned, unsigned long *, source_program_t *);
const char *macro_actuals(const char *, char [32][256], size_t *, macro_eval_t, void *, unsigned);
const char *macro_condition(const statement_t *, int *);
void macro_join(char *);
const char *macro_arguments(const char *, char [32][256], size_t *);
const char *macro_block(macro_t **, const source_program_t *, size_t, size_t, const char *);
const char *macro_iteration(const macro_t *, const char *, unsigned long *, source_program_t *);
void macro_destroy(macro_t *);
int macro_reserved(const char *);
#endif
