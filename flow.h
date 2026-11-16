#ifndef flow_h
#define flow_h

#include "dialect.h"

typedef enum flow_kind {
	flow_next, flow_branch, flow_call, flow_return, flow_indirect, flow_barrier
} flow_kind_t;

typedef struct flow_node {
	flow_kind_t kind;
	long next; /* Fall-through/call continuation; -1 means no known sequential successor. */
	long target; /* Taken edge; -1 means unresolved (or inapplicable for flow_next). */
	int conditional;
} flow_node_t;

typedef struct flow_graph {
	flow_node_t *nodes;
	size_t count;
} flow_graph_t;

/* Borrow immutable source, allocate one node per source record. Labels do
 * not split routines: ordinary fall-through crosses them. Only unique,
 * literal code labels resolve. A call continuation is NOT an edge proving
 * that a callee returns there; stack/return ownership is deliberately unknown.
 * Initialize output to zero; destroy even after failure. Nonzero is success. */
int flow_build(const source_program_t *, flow_graph_t *);
void flow_destroy(flow_graph_t *);

#endif
