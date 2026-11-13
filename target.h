#ifndef target_h
#define target_h

#include "emitter.h"

/* An owned target program can be rendered repeatedly after lowering ends. */
typedef struct target_program {
	output_t *events;
	size_t count;
	size_t capacity;
	int failed;
	const char *error;
	unsigned long error_line;
} target_program_t;

emitter_t target_builder(target_program_t *);
int target_write(const target_program_t *, const emitter_t *);
void target_destroy(target_program_t *);

#endif
