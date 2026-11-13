#ifndef recognition_h
#define recognition_h

#include "dialect.h"

/* Recognition records the proven input C at each source instruction.
 * It does not select an OS or alter the source instructions. */
typedef long (*resolve_value_t)(void *, const char *);
int recognize_os_calls(const source_program_t *, resolve_value_t, void *, long **);

#endif
