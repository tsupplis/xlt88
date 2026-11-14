#ifndef fcbaddr_h
#define fcbaddr_h

#include "dialect.h"

/* Borrowed source expression and its defining record. This proves only the
 * DE value at a call, not that the pointed-to storage is safe to repurpose. */
typedef struct fcb_address {
	const char *expression;
	size_t definition;
} fcb_address_t;

int fcb_address_before(const source_program_t *, size_t, fcb_address_t *);

#endif
