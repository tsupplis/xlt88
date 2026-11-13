#ifndef dialect_h
#define dialect_h

#include <stdio.h>

typedef struct statement {
	char label[256];
	char opcode[256];
	char first[256];
	char second[256];
	char comment[256];
} statement_t;

typedef struct source_record {
	statement_t statement;
	const char *problem;
	unsigned long line;
} source_record_t;

typedef struct source_program {
	source_record_t *records;
	size_t count;
} source_program_t;

typedef struct dialect {
	const char *name;
	int (*read)(FILE *, source_program_t *);
} dialect_t;

int emit_normalized_8080(const source_program_t *, FILE *, FILE *);
void source_destroy(source_program_t *);

extern const dialect_t intel_dialect;

#endif
