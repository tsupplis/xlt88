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
	output->short_branch = event->short_branch;
	output->text = copy_text(event->text);
	if (event->text && !output->text)
		program->failed = 1;
	for (i = 0; i < 2; ++i) {
		operand_t *dest = &output->operands[i];
		const operand_t *source = &event->operands[i];
		dest->kind = source->kind;
		dest->value = source->value;
		dest->source_line = source->source_line;
		dest->text = copy_text(source->text);
		if (source->text && !dest->text)
			program->failed = 1;
		if (source->resolve) {
			const char *error;
			dest->expression = expr_parse(source->text, source->resolve, source->symbols,
			    event->kind == output_data_bytes || event->kind == output_data_words, &error);
			if (!dest->expression) {
				program->failed = 1;
				program->error = error;
				program->error_line = source->source_line;
			}
		}
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
	size_t i, j;

	for (i = 0; i < program->count; ++i) {
		output_t *event = &program->events[i];
		free((void *)event->text);
		for (j = 0; j < 2; ++j) {
			operand_t *operand = &event->operands[j];
			free((void *)operand->text);
			expr_destroy(operand->expression);
		}
	}
	free(program->events);
	memset(program, 0, sizeof(*program));
}
