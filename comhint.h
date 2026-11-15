#ifndef comhint_h
#define comhint_h

/* Candidate discovery only: calls invalidate registers but assume balanced
 * caller stack storage. A recovered return target is a hint, not a port proof. */
typedef struct com_hint {
	int reg[8];
	int stack[16];
	unsigned depth;
} com_hint_t;

void com_hint_init(com_hint_t *);
int com_hint_step(com_hint_t *, const unsigned char *);
int com_hint_read(com_hint_t *, const unsigned char *, const unsigned char *, unsigned, unsigned);
int com_ptr_used(const unsigned char *, const unsigned char *, const unsigned char *, unsigned, unsigned);

#endif
