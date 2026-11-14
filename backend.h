#ifndef backend_h
#define backend_h

#include "emitter.h"

/* A native far result is emitted, but its source consumers need a port review. */
typedef enum bdos_result {
	bdos_unsupported = 0,
	bdos_lowered = 1,
	bdos_far_result = 2,
	bdos_buffered_result = 3
} bdos_result_t;

/* Hooks borrow the output builder. They emit typed events, not assembly text. */
typedef struct backend {
	const char *name;
	/* Return a bdos_result_t; far results require a caller diagnostic. */
	int (*bdos)(const emitter_t *, long function, unsigned long site);
	void (*exit_program)(const emitter_t *);
	/* If present, emit segment selection immediately before the CPU layer
	 * emits MOV SP; no intervening instructions may be inserted. */
	void (*stack_segment)(const emitter_t *);
	/* Invoked only for a validated CP/M FCB lowering plan. */
	int (*file_call)(const emitter_t *, long function, unsigned long site);
	/* Optional native fallback: selector is already in the target register.
	 * No source-contract adaptation or segment preservation is implied. */
	void (*native_bdos)(const emitter_t *);
} backend_t;

extern const backend_t msdos_backend;
extern const backend_t cpm86_backend;

#endif
