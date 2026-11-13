#ifndef BACKEND_H
#define BACKEND_H

#include <stdio.h>

struct backend {
	const char *name;
	/* Return zero when the service has no direct translation. */
	int (*bdos)(FILE *out, long function);
	void (*exit_program)(FILE *out);
	void (*stack_segment)(FILE *out);
};

extern const struct backend msdos_backend;
extern const struct backend cpm86_backend;

#endif
