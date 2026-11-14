#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "osrecog.h"

/* An injected resolver makes the analysis test independent of parsing,
 * symbol storage, OS backends and assembler availability. */
static long
resolve(void *context, const char *text)
{
	char *end;
	long value;
	if (!strcmp(text, "selector")) return 512;
	if (!strcmp(text, "print_service"))
		return *(const long *)context;
	value = strtol(text, &end, 10);
	return *text && !*end ? value : -1;
}

int
main(void)
{
	source_record_t records[] = {
		{ .statement = { .opcode = "mvi", .first = "A", .second = "2" } },
		{ .statement = { .opcode = "call", .first = "wrapper" } },
		{ .statement = { .opcode = "mvi", .first = "a", .second = "print_service" } },
		{ .statement = { .opcode = "call", .first = "wrapper" } },
		{ .statement = { .opcode = "jmp", .first = "0" } },
		{ .statement = { .label = "wrapper", .opcode = "mov", .first = "C", .second = "A" } },
		{ .statement = { .opcode = "jmp", .first = "5" } }
	};
	source_record_t original[sizeof(records) / sizeof(*records)];
	source_program_t source = { .records = records, .count = sizeof(records) / sizeof(*records) };
	os_values_t *sets = NULL;
	long print_service = 9;

	memcpy(original, records, sizeof(records));
	assert(recognize_os_sets(&source, resolve, &print_service, &sets));
	assert(sets[6].count == 2);
	assert((sets[6].values[0] == 2 && sets[6].values[1] == 9) ||
	    (sets[6].values[0] == 9 && sets[6].values[1] == 2));
	assert(sets[2].count == 0); /* No invented call-return convention. */
	assert(!memcmp(original, records, sizeof(records)));
	free(sets);

	print_service = -1;
	assert(recognize_os_sets(&source, resolve, &print_service, &sets));
	assert(sets[6].count == 0); /* An unknown caller poisons the join. */
	assert(!memcmp(original, records, sizeof(records)));
	free(sets);
	{
		source_record_t memory[] = {
			{ .statement = { .opcode = "mvi", .first = "a", .second = "2" } },
			{ .statement = { .opcode = "sta", .first = "selector" } },
			{ .statement = { .opcode = "call", .first = "clobber" } },
			{ .statement = { .opcode = "lda", .first = "selector" } },
			{ .statement = { .opcode = "mov", .first = "c", .second = "a" } },
			{ .statement = { .opcode = "call", .first = "5" } },
			{ .statement = { .opcode = "jmp", .first = "0" } },
			{ .statement = { .label = "clobber", .opcode = "ret" } },
			{ .statement = { .label = "selector", .opcode = "ds", .first = "1" } }
		};
		source_record_t copy[sizeof(memory) / sizeof(*memory)];
		source_program_t program = { .records = memory, .count = sizeof(memory) / sizeof(*memory) };
		long *single = NULL;
		memcpy(copy, memory, sizeof(memory));
		assert(recognize_os_sets(&program, resolve, &print_service, &sets));
		assert(sets[5].count == 1 && sets[5].values[0] == 2 && sets[5].guarded);
		assert(!memcmp(copy, memory, sizeof(memory)));
		free(sets);
		assert(recognize_os_calls(&program, resolve, &print_service, &single));
		assert(single[5] == -1); /* A guarded singleton is not a proof. */
		free(single);
		strcpy(memory[8].statement.first, "2");
		assert(recognize_os_sets(&program, resolve, &print_service, &sets));
		assert(!sets[5].count); /* No guessed array/table selector. */
		free(sets);
	}
	return 0;
}
