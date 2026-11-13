#ifndef backend_h
#define backend_h

#include "emitter.h"

typedef struct backend {
	const char *name;
	/* Return zero when the service has no direct translation. */
	int (*bdos)(const emitter_t *, long function);
	void (*exit_program)(const emitter_t *);
	void (*stack_segment)(const emitter_t *);
} backend_t;

extern const backend_t msdos_backend;
extern const backend_t cpm86_backend;

#endif
