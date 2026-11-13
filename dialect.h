#ifndef DIALECT_H
#define DIALECT_H

struct statement {
	char label[256];
	char opcode[256];
	char first[256];
	char second[256];
	char comment[256];
};

struct dialect {
	const char *name;
	const char *(*parse)(const char *, struct statement *);
};

extern const struct dialect intel_dialect;

#endif
