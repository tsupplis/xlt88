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
	char text[255]; /* Reader text for lexical macro substitution. */
	const char *problem;
	unsigned long line; /* Stable expanded-record ordinal for current passes. */
	unsigned long origin_line;
	char *origin_file; /* Owned; NULL for stream-only sources. */
} source_record_t;

typedef struct source_program {
	source_record_t *records;
	size_t count;
	char **files; /* Owned input paths, including transitive includes. */
	size_t file_count;
} source_program_t;

typedef struct dialect {
	const char *name;
	int (*read)(FILE *, source_program_t *);
} dialect_t;

int source_read(const dialect_t *, FILE *, const char *, source_program_t *);
const char *m80_statement(const char *, statement_t *, int);
int m80_raw_read(FILE *, source_program_t *);
int m80_read_file(FILE *, const char *, source_program_t *);
int mac_literals(char [256]);
int mac_expression(const char *);
const char *mac_names(char *, int);
int mac_read_file(FILE *, const char *, source_program_t *);
int mac_raw_read(FILE *, source_program_t *);
const char *rmac_statement(const char *, statement_t *);
int rmac_read_file(FILE *, const char *, source_program_t *);
typedef enum macro_mode {
	macro_m80, macro_mac, macro_rmac
} macro_mode_t;

/* Shared preprocessing borrows the raw reader. MAC/RMAC modes use immutable
 * EQU versions; RMAC also retains module directives for its placement pass. */
int macro_read_file(FILE *, const char *, source_program_t *,
    int (*)(FILE *, source_program_t *), macro_mode_t);
void source_report(FILE *, const source_record_t *, const char *);

int emit_normalized_8080(const source_program_t *, FILE *, FILE *);
void source_destroy(source_program_t *);

extern const dialect_t asm80_dialect;
extern const dialect_t m80_dialect;
extern const dialect_t mac_dialect;
extern const dialect_t rmac_dialect;

#endif
