#ifndef comread_h
#define comread_h

#include "dialect.h"

/* Borrow both binary and annotation streams. The result owns ordinary source
 * records and feeds the same symbol/lowering pipeline as assembly readers. */
int com_read(FILE *, FILE *, source_program_t *, FILE *);

#endif
