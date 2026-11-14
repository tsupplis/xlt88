#ifndef reset_h
#define reset_h

#include "emitter.h"

/* Inject the native call ABI; generated data lives in the program segment.
 * The callback emits a complete call and line ending. No runtime OS model. */
typedef void (*reset_call_t)(const emitter_t *, long);
void reset_disks(const emitter_t *, reset_call_t, int, unsigned long);

#endif
