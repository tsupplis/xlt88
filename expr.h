#ifndef expr_h
#define expr_h

#include <stddef.h>

typedef enum expr_kind {
	expr_integer, expr_string, expr_symbol, expr_location,
	expr_unary, expr_binary, expr_list
} expr_kind_t;
typedef enum expr_op {
	expr_plus, expr_minus, expr_multiply, expr_divide, expr_modulo,
	expr_and, expr_or, expr_xor, expr_left, expr_right, expr_complement
} expr_op_t;
typedef struct expr {
	expr_kind_t kind;
	expr_op_t op;
	long integer;
	size_t symbol_id;
	int is_equ;
	char *text;
	struct expr *left, *right;
} expr_t;

/* Resolver returns a borrowed preferred spelling; the tree copies it.
 * Unresolved names are retained with ID zero for forward layout analysis. */
typedef const char *(*expr_resolve_t)(void *, const char *, int *, size_t *);
typedef int (*expr_lookup_t)(void *, size_t, const char *, long *);
expr_t *expr_parse(const char *, expr_resolve_t, void *, int list,
    const char **error);
void expr_destroy(expr_t *);
/* Return nonzero only for a known value without invalid arithmetic/overflow.
 * Location is -1 when unknown; lookup must bound recursive EQU evaluation. */
int expr_eval(const expr_t *, expr_lookup_t, void *, long location, long *);

#endif
