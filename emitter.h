#ifndef emitter_h
#define emitter_h

#include <stdio.h>

typedef enum operand_kind {
	operand_register, operand_interrupt, operand_immediate, operand_expression, operand_memory, operand_byte_memory, operand_branch, operand_internal_label
} operand_kind_t;

typedef enum expression_part_kind {
	expression_text, expression_equ, expression_address, expression_and
} expression_part_kind_t;

typedef struct expression_part {
	expression_part_kind_t kind;
	char *text;
} expression_part_t;

typedef struct operand {
	operand_kind_t kind;
	const char *text;
	long value;
	const char *(*resolve)(void *, const char *, int *);
	void *symbols;
	expression_part_t *parts;
	size_t part_count;
} operand_t;

typedef enum output_kind {
	output_header, output_instruction, output_label, output_internal_definition, output_equate, output_data_bytes,
	output_data_words, output_reserve_bytes, output_alignment, output_comment, output_line_end, output_error_comment
} output_kind_t;

typedef struct output {
	output_kind_t kind;
	const char *text;
	operand_t operands[2];
} output_t;

/* Events and their strings are borrowed for the duration of the callback. */
typedef struct emitter {
	void *context;
	void (*write)(void *context, const output_t *);
} emitter_t;

void emit(const emitter_t *, output_kind_t, const char *, operand_t,
    operand_t);
void instruction(const emitter_t *, const char *, operand_t, operand_t);

#define no_operand ((operand_t){0})
#define operand(type, name) ((operand_t){.kind = type, .text = name})
#define value(type, number) ((operand_t){.kind = type, .value = number})

extern const emitter_t nasm_emitter;

#endif
