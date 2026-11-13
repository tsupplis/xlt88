#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "target.h"

static char *
copy_text(const char *text)
{
	size_t length;
	char *copy;

	if (!text)
		return NULL;
	length = strlen(text) + 1;
	copy = malloc(length);
	if (copy)
		memcpy(copy, text, length);
	return copy;
}

static void
copy_expression(target_program_t *program, operand_t *output, const operand_t *input)
{
	const char *text = input->text;
	size_t capacity = strlen(text) + 1;

	output->parts = calloc(capacity, sizeof(*output->parts));
	if (!output->parts) {
		program->failed = 1;
		return;
	}
	while (*text) {
		const char *start = text++;
		const char *name = NULL;
		int is_equ = 0;
		expression_part_t *part = &output->parts[output->part_count++];

		if (*start == '\'' || *start == '"') {
			while (*text && *text++ != *start)
				;
		} else if (isalnum((unsigned char)*start) || strchr("_.$?@#", *start)) {
			char token[256];
			while (*text && (isalnum((unsigned char)*text) ||
			    strchr("_.$?@#", *text)))
				++text;
			if ((size_t)(text - start) >= sizeof(token)) {
				program->failed = 1;
				return;
			}
			memcpy(token, start, text - start);
			token[text - start] = '\0';
			name = input->resolve(input->symbols, token, &is_equ);
			if (!name && strlen(token) == 3 &&
			    tolower((unsigned char)token[0]) == 'a' &&
			    tolower((unsigned char)token[1]) == 'n' &&
			    tolower((unsigned char)token[2]) == 'd')
				part->kind = expression_and;
		}
		if (name) {
			part->kind = is_equ ? expression_equ : expression_address;
			part->text = copy_text(name);
		} else {
			size_t length = text - start;
			part->text = malloc(length + 1);
			if (part->text) {
				memcpy(part->text, start, length);
				part->text[length] = '\0';
			}
		}
		if (!part->text) {
			program->failed = 1;
			return;
		}
	}
}

static void
append_event(void *context, const output_t *event)
{
	target_program_t *program = context;
	output_t *output;
	size_t i;

	if (program->failed)
		return;
	if (program->count == program->capacity) {
		size_t capacity = program->capacity ? program->capacity * 2 : 64;
		void *events = realloc(program->events, capacity * sizeof(*output));
		if (!events) {
			program->failed = 1;
			return;
		}
		program->events = events;
		program->capacity = capacity;
	}
	output = &program->events[program->count++];
	memset(output, 0, sizeof(*output));
	output->kind = event->kind;
	output->text = copy_text(event->text);
	if (event->text && !output->text)
		program->failed = 1;
	for (i = 0; i < 2; ++i) {
		operand_t *dest = &output->operands[i];
		const operand_t *source = &event->operands[i];
		dest->kind = source->kind;
		dest->value = source->value;
		dest->text = copy_text(source->text);
		if (source->text && !dest->text)
			program->failed = 1;
		if (source->resolve)
			copy_expression(program, dest, source);
	}
}

emitter_t
target_builder(target_program_t *program)
{
	emitter_t builder = { program, append_event };
	return builder;
}

int
target_write(const target_program_t *program, const emitter_t *emitter)
{
	size_t i;

	if (program->failed)
		return 0;
	for (i = 0; i < program->count; ++i)
		emitter->write(emitter->context, &program->events[i]);
	return 1;
}

void
target_destroy(target_program_t *program)
{
	size_t i, j, k;

	for (i = 0; i < program->count; ++i) {
		output_t *event = &program->events[i];
		free((void *)event->text);
		for (j = 0; j < 2; ++j) {
			operand_t *operand = &event->operands[j];
			free((void *)operand->text);
			for (k = 0; k < operand->part_count; ++k)
				free(operand->parts[k].text);
			free(operand->parts);
		}
	}
	free(program->events);
	memset(program, 0, sizeof(*program));
}
