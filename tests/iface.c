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

/* Every eligible common body must match the individual backend lowering,
 * excluding comments. Special services must not enter this equivalence class. */
static void
shared_services(void)
{
	const unsigned char excluded[] = {0, 6, 13, 27, 31, 38, 39, 41, 255};
	unsigned char values[] = {2, 0};
	for (size_t i = 0; i < sizeof(excluded); ++i) {
		values[1] = excluded[i];
		assert(!cpm86_backend.shared_bdos(NULL, values, 2));
	}
	for (int f = 1; f <= 40; ++f) {
		target_program_t common = {0}, single = {0};
		emitter_t a = target_builder(&common), b = target_builder(&single);
		size_t j = 0;
		values[1] = (unsigned char)f;
		if (!cpm86_backend.shared_bdos(NULL, values, 2)) continue;
		assert(cpm86_backend.shared_bdos(&a, values, 2));
		assert(cpm86_backend.bdos(&b, f, 0) == bdos_lowered);
		for (size_t i = 0; i < single.count; ++i) {
			const output_t *event = &single.events[i];
			if (event->kind != output_instruction) continue;
			while (j < common.count && common.events[j].kind != output_instruction) ++j;
			assert(j < common.count);
			assert(!strcmp(event->text, common.events[j].text));
			for (int k = 0; k < 2; ++k) {
				const operand_t *x = &event->operands[k], *y = &common.events[j].operands[k];
				assert(x->kind == y->kind && x->value == y->value);
				assert(x->text && y->text ? !strcmp(x->text, y->text) : x->text == y->text);
			}
			++j;
		}
		while (j < common.count && common.events[j].kind != output_instruction) ++j;
		assert(j == common.count);
		target_destroy(&common);
		target_destroy(&single);
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
	shared_services();
	assert(input && normalized && assembly);
	fputs(source, input);
	rewind(input);
	assert(asm80_dialect.read(input, &program));
	snapshot = malloc(program.count * sizeof(*snapshot));
	assert(snapshot);
	memcpy(snapshot, program.records, program.count * sizeof(*snapshot));
	assert(emit_normalized_8080(&program, normalized, stderr));
	rewind(normalized);
	assert(asm80_dialect.read(normalized, &reparsed));
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
