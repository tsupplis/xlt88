#ifndef backend_h
#define backend_h

#include "emitter.h"

/* Hooks borrow the output builder. They emit typed events, not assembly text. */
typedef struct backend {
	const char *name;
	/* Return zero when the service has no direct translation. */
	int (*bdos)(const emitter_t *, long function, unsigned long site);
	void (*exit_program)(const emitter_t *);
	/* If present, emit segment selection immediately before the CPU layer
	 * emits MOV SP; no intervening instructions may be inserted. */
	void (*stack_segment)(const emitter_t *);
	/* Invoked only for a statically proven CP/M FCB use. */
	int (*file_call)(const emitter_t *, long function, unsigned long site);
} backend_t;

extern const backend_t msdos_backend;
extern const backend_t cpm86_backend;

#endif
