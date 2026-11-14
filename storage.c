#include <string.h>
#include "storage.h"

int
storage_find(const source_program_t *source, const symbol_table_t *symbols,
    const symbol_t *object, size_t *record, long *size)
{
	size_t i;
	if (!object || object->is_equ || !object->line || object->source_address < 0)
		return 0;
	i = object->line - 1;
	while (i < source->count && !source->records[i].statement.opcode[0]) {
		if (i != object->line - 1 && source->records[i].statement.label[0]) return 0;
		++i;
	}
	if (i >= source->count || (i != object->line - 1 && source->records[i].statement.label[0]) ||
	    strcmp(source->records[i].statement.opcode, "ds") ||
	    !symbols_eval(symbols, source->records[i].statement.first, -1, size) ||
	    *size < 0 || *size > 65536 - object->source_address) return 0;
	*record = i;
	return 1;
}
