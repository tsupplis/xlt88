#include "emitter.h"

void
emit(const emitter_t *emitter, output_kind_t kind, const char *text,
    operand_t first, operand_t second)
{
	const output_t event = { kind, text, {first, second}, 0 };
	emitter->write(emitter->context, &event);
}

void
instruction(const emitter_t *emitter, const char *mnemonic,
    operand_t first, operand_t second)
{
	emit(emitter, output_instruction, mnemonic, first, second);
}
