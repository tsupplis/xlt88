#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "trans.h"
#include "asm86.h"
#include "masm.h"
#include "drasm86.h"

static void
inspect_expression(const expr_t *tree)
{
	if (!tree) return;
	if (tree->kind == expr_symbol) {
		assert(tree->symbol_id > 0);
		assert(tree->text && tree->text[0]);
	}
	inspect_expression(tree->left);
	inspect_expression(tree->right);
}

static void
inspect(void *context, const output_t *event)
{
	size_t *count = context;
	size_t i;
	++*count;
	for (i = 0; i < 2; ++i) {
		assert(event->operands[i].resolve == NULL);
		assert(event->operands[i].symbols == NULL);
		inspect_expression(event->operands[i].expression);
	}
}

int
main(void)
{
	const char *source = "bdos equ 5\nstart: lxi d,message\n mvi c,9\n call bdos\n jmp 0\nmessage: db 'hello$'\n end\n";
	source_program_t program = {0}, reparsed = {0};
	target_program_t dos = {0}, cpm = {0};
	symbol_table_t symbols = {0};
	FILE *input = tmpfile(), *normalized = tmpfile(), *assembly = tmpfile();
	source_record_t *snapshot;
	size_t count = 0, i;
	emitter_t observer = { &count, inspect }, nasm = nasm_emitter;
	assert(input && normalized && assembly);
	fputs(source, input);
	rewind(input);
	assert(intel_dialect.read(input, &program));
	snapshot = malloc(program.count * sizeof(*snapshot));
	assert(snapshot);
	memcpy(snapshot, program.records, program.count * sizeof(*snapshot));
	assert(emit_normalized_8080(&program, normalized, stderr));
	rewind(normalized);
	assert(intel_dialect.read(normalized, &reparsed));
	assert(program.count == reparsed.count);
	for (i = 0; i < program.count; ++i)
		assert(!memcmp(&program.records[i].statement,
		    &reparsed.records[i].statement, sizeof(statement_t)));
	assert(symbols_build(&program, &symbols, stderr));
	assert(!lower_named(&program, &symbols, &msdos_backend, &dos, stderr));
	assert(!lower_named(&program, &symbols, &cpm86_backend, &cpm, stderr));
	symbols_destroy(&symbols);
	assert(!memcmp(snapshot, program.records, program.count * sizeof(*snapshot)));
	free(snapshot);
	source_destroy(&program);
	source_destroy(&reparsed);
	/* Replay after both source and lowering contexts have been destroyed. */
	assert(target_write(&dos, &observer));
	assert(count == dos.count);
	nasm.context = assembly;
	assert(target_write(&dos, &nasm));
	assert(target_write(&cpm, &nasm));
	assert(asm86_render(&dos, assembly, stderr));
	assert(masm_render(&dos, assembly, stderr));
	assert(drasm86_render(&dos, assembly, stderr));
	assert(drasm86_render(&cpm, assembly, stderr));
	assert(masm_render(&cpm, assembly, stderr));
	assert(asm86_render(&cpm, assembly, stderr));
	/* A second replay must not consume or mutate the owned target. */
	assert(target_write(&cpm, &nasm));
	assert(asm86_render(&cpm, assembly, stderr));
	assert(!ferror(assembly));
	target_destroy(&dos);
	target_destroy(&cpm);
	fclose(input);
	fclose(normalized);
	fclose(assembly);
	return 0;
}
