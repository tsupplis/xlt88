#include <stdlib.h>
#include <string.h>
#include "fcb.h"
#include "fcbaddr.h"
#include "fcbapi.h"
#include "fcbdef.h"
#include "storage.h"

static int
equal(const char *a, const char *b)
{
	return symbol_equal(a, b);
}

static long
address_value(const symbol_table_t *symbols, const char *text)
{
	long value;
	return symbols_eval(symbols, text, -1, &value) ? value : -1;
}

static size_t
previous(const source_program_t *source, size_t i)
{
	while (i) {
		--i;
		if (source->records[i].statement.opcode[0] || source->records[i].statement.label[0]) return i;
	}
	return source->count;
}

static const symbol_t *
binding(const symbol_table_t *symbols, const char *text)
{
	const char *alias = symbol_alias(symbols, text);
	return alias ? symbol_find(symbols, alias) : NULL;
}

static long
object_size(const source_program_t *source, const symbol_table_t *symbols, const symbol_t *object)
{
	long size = 0;
	size_t record;
	if (!storage_find(source, symbols, object, &record, &size)) return 0;
	return size;
}

/* R0..R2 are a shared 24-bit, 128-byte-record field. Accept only a
 * relocatable object-plus-constant expression, never a numeric source address.
 * Other metadata and indirect aliases retain the existing storage audit. */
static const symbol_t *
record_read(const symbol_table_t *symbols, const char *text, int width)
{
	const char *error = NULL;
	const symbol_t *object = NULL;
	expr_t *tree = expr_parse(text, NULL, NULL, 0, &error);
	if (tree && tree->kind == expr_binary && tree->op == expr_plus) {
		const expr_t *name = tree->left, *offset = tree->right;
		if (name->kind == expr_integer) { name = tree->right; offset = tree->left; }
		if (name->kind == expr_symbol && offset->kind == expr_integer &&
		    offset->integer >= 33 && offset->integer <= 36 - width)
			object = binding(symbols, name->text);
	}
	expr_destroy(tree);
	return object;
}

static int
transfer(const statement_t *stmt)
{
	return stmt->opcode[0] == 'j' || equal(stmt->opcode, "call") ||
	    equal(stmt->opcode, "cz") || equal(stmt->opcode, "cnz") ||
	    equal(stmt->opcode, "cc") || equal(stmt->opcode, "cnc") ||
	    equal(stmt->opcode, "cp") || equal(stmt->opcode, "cm") ||
	    equal(stmt->opcode, "cpe") || equal(stmt->opcode, "cpo");
}

/* Recognize the bounded semantics of a 12-byte name copy, not a routine name.
 * Labels inside the region may only be entered through its own loop edge. */
static int
name_copy(const source_program_t *source, size_t start)
{
	static const char *const ops[] = {"mvi", "mov", "stax", "inx", "inx", "dcr", "jnz", "ret"};
	static const char *const first[] = {"b", "a", "d", "h", "d", "b"};
	size_t j;
	if (start + 8 > source->count) return 0;
	for (j = 0; j < 8; ++j) {
		const statement_t *stmt = &source->records[start + j].statement;
		if (!equal(stmt->opcode, ops[j])) return 0;
		if (j < 6 && !equal(stmt->first, first[j])) return 0;
		if (j == 0 && symbol_number(stmt->second) != 12) return 0;
		if (j == 1 && !equal(stmt->second, "m")) return 0;
		if (j > 1 && stmt->second[0]) return 0;
		if (j == 6 && (!source->records[start + 1].statement.label[0] ||
		    !equal(stmt->first, source->records[start + 1].statement.label))) return 0;
		if (j == 7 && stmt->first[0]) return 0;
	}
	return 1;
}

void
fcb_destroy(fcb_plan_t *plan)
{
	free(plan->calls);
	free(plan->grow);
	memset(plan, 0, sizeof(*plan));
}

int
fcb_analyze(const source_program_t *source, const symbol_table_t *symbols,
    const long *functions, fcb_plan_t *plan, FILE *diagnostics)
{
	unsigned char *objects = NULL, *allowed = NULL;
	size_t *regions = NULL;
	size_t i, j, count = source->count;
	const char *problem = "cannot allocate FCB analysis";
	int found = 0, result = 0;
	plan->calls = calloc(count ? count : 1, 1);
	plan->grow = calloc(count ? count : 1, 1);
	objects = calloc(symbols->count ? symbols->count : 1, 1);
	allowed = calloc(count ? count : 1, 1);
	regions = calloc(count ? count : 1, sizeof(*regions));
	if (!plan->calls || !plan->grow || !objects || !allowed || !regions) goto done;
	result = fcb_default_plan(source, symbols, functions, plan->calls, &problem);
	if (result) { result = result > 0; goto done; }
	/* A call must receive an exact object address through a proven-safe
	 * straight-line path; metadata safety is checked separately below. */
	for (i = 0; i < count; ++i) {
		const statement_t *stmt = &source->records[i].statement;
		size_t load;
		const symbol_t *object;
		if (!transfer(stmt) || address_value(symbols, stmt->first) != 5 || !fcb_function(functions[i])) continue;
		found = 1;
		{
			fcb_address_t address;
			long value;
			problem = "file call needs a proven FCB address in DE";
			if (!fcb_address_before(source, i, &address)) goto done;
			load = address.definition;
			if (symbols_eval(symbols, address.expression, -1, &value) &&
			    (value == 0x5c || value == 0x6c)) {
				problem = "default CP/M FCB recognized; native storage and whole-program field-use proof are required";
				goto done;
			}
		}
		stmt = &source->records[load].statement;
		object = binding(symbols, stmt->second);
		if (!object || object->is_equ) goto done;
		objects[object->id - 1] = 1;
		allowed[load] = 1;
		plan->calls[i] = 1;
	}
	if (!found) { result = 1; goto done; }
	/* Require a dedicated zero-initialized reservation. 33-byte CP/M FCBs
	 * grow to 36 bytes; symbolic addressing relocates following objects. */
	for (j = 0; j < symbols->count; ++j) if (objects[j]) {
		const symbol_t *object = &symbols->items[j];
		long size;
		problem = "FCB must have dedicated DS storage of at least 33 bytes";
		if (!storage_find(source, symbols, object, &i, &size) || size < 33) goto done;
		plan->grow[i] = size < 36;
		allowed[i] = 1;
		/* Another label at the object address is an untracked alias. */
		for (size_t k = 0; k < symbols->count; ++k)
			if (k != j && !symbols->items[k].is_equ && object->source_address >= 0 &&
			    symbols->items[k].source_address == object->source_address) {
				problem = "FCB storage has an untracked label alias"; goto done;
			}
	}
	/* Discover and verify all calls to bounded name-copy routines. */
	for (i = 0; i < count; ++i) {
		const statement_t *stmt = &source->records[i].statement;
		const symbol_t *routine, *object;
		size_t load, origin, start;
		long address;
		if (!equal(stmt->opcode, "call") || address_value(symbols, stmt->first) == 5) continue;
		problem = "unrecognized subroutine may expose FCB storage";
		routine = binding(symbols, stmt->first);
		if (!routine || routine->is_equ) goto done;
		start = routine->line - 1;
		if (!name_copy(source, start)) goto done;
		load = previous(source, i); origin = previous(source, load);
		if (load == count || origin == count || stmt->label[0] || source->records[load].statement.label[0]) goto done;
		stmt = &source->records[load].statement;
		object = binding(symbols, stmt->second);
		if (!equal(stmt->opcode, "lxi") || !equal(stmt->first, "d") || !object || !objects[object->id - 1]) goto done;
		stmt = &source->records[origin].statement;
		if (!equal(stmt->opcode, "lxi") || !equal(stmt->first, "h") ||
		    !symbols_eval(symbols, stmt->second, -1, &address) || (address != 0x5c && address != 0x6c)) goto done;
		allowed[load] = allowed[i] = 1;
		for (j = start; j < start + 8; ++j) regions[j] = start + 1;
	}
	for (i = 0; i < count; ++i) {
		const statement_t *stmt = &source->records[i].statement;
		const symbol_t *dest;
		/* No control transfer may enter the middle of the audited copier. */
		if (transfer(stmt) && (dest = binding(symbols, stmt->first)) && !dest->is_equ) {
			size_t entry = dest->line - 1;
			while (entry < count && !source->records[entry].statement.opcode[0]) ++entry;
			if (entry < count && (equal(source->records[entry].statement.opcode, "db") ||
			    equal(source->records[entry].statement.opcode, "dw") || equal(source->records[entry].statement.opcode, "ds"))) {
				problem = "transfer to data cannot be audited"; goto done;
			}
			if (entry < count && regions[entry] &&
			    !(allowed[i] && equal(stmt->opcode, "call") && entry + 1 == regions[entry]) &&
			    !(regions[i] == regions[entry] && equal(stmt->opcode, "jnz") && entry == regions[entry])) {
				problem = "control transfer enters audited name-copy routine"; goto done;
			}
		}
		if (regions[i]) {
			if (regions[i] == i + 1) {
				size_t before = previous(source, i);
				if (before == count || !equal(source->records[before].statement.opcode, "jmp")) {
					problem = "name-copy routine can be entered by fallthrough"; goto done;
				}
			}
			continue;
		}
		if (equal(stmt->opcode, "ldax") || equal(stmt->opcode, "ret") ||
		    (stmt->opcode[0] == 'r' && strlen(stmt->opcode) <= 3 &&
		    !equal(stmt->opcode, "rlc") && !equal(stmt->opcode, "rrc") &&
		    !equal(stmt->opcode, "ral") && !equal(stmt->opcode, "rar") && !equal(stmt->opcode, "rst"))) {
			problem = "unproved indirect memory or return in FCB program"; goto done;
		}
		/* All indirect reads outside the copier need a straight-line address
		 * definition. This prevents synthesized aliases to repurposed fields. */
		if ((equal(stmt->opcode, "mov") && equal(stmt->second, "m")) ||
		    equal(stmt->first, "m")) {
			size_t load = previous(source, i);
			long offset = 0, address;
			if (stmt->label[0]) { problem = "indirect read has an alternate entry"; goto done; }
			while (load != count && equal(source->records[load].statement.opcode, "inx") &&
			    equal(source->records[load].statement.first, "h") && !source->records[load].statement.label[0]) {
				++offset; load = previous(source, load);
			}
			problem = "indirect read address is not proven outside FCB metadata";
			if (load == count || !equal(source->records[load].statement.opcode, "lxi") ||
			    !equal(source->records[load].statement.first, "h") ||
			    !binding(symbols, source->records[load].statement.second) ||
			    !symbols_eval(symbols, source->records[load].statement.second, -1, &address)) goto done;
			for (j = 0; j < symbols->count; ++j) if (objects[j]) {
				long base = symbols->items[j].source_address;
				if (base < 0 || (address + offset >= base && address + offset < base + object_size(source, symbols, &symbols->items[j]))) goto done;
			}
		}
		if (equal(stmt->opcode, "lda") || equal(stmt->opcode, "lhld")) {
			const symbol_t *address = binding(symbols, stmt->first);
			const symbol_t *record = record_read(symbols, stmt->first,
			    equal(stmt->opcode, "lhld") ? 2 : 1);
			if (record && !record->is_equ && objects[record->id - 1] &&
			    object_size(source, symbols, record) >= 36) allowed[i] = 1;
			if (!allowed[i] && (!address || address->is_equ || address->source_address < 0)) {
				problem = "direct memory address cannot be audited for FCB aliases"; goto done;
			}
		}
		if (equal(stmt->opcode, "lxi") && equal(stmt->first, "sp")) {
			const symbol_t *top = binding(symbols, stmt->second);
			size_t reserve = top ? previous(source, top->line - 1) : count;
			long size;
			problem = "stack storage is not proven separate from FCBs";
			if (reserve == count || !equal(source->records[reserve].statement.opcode, "ds") ||
			    !symbols_eval(symbols, source->records[reserve].statement.first, -1, &size) || size < 128) goto done;
		}
		if ((equal(stmt->opcode, "inx") || equal(stmt->opcode, "dcx")) && equal(stmt->first, "sp")) {
			problem = "stack pointer arithmetic cannot be audited"; goto done;
		}
		if (transfer(stmt) && address_value(symbols, stmt->first) == 5 && functions[i] == 26) {
			size_t load = previous(source, previous(source, i));
			const symbol_t *buffer = load < count ? binding(symbols, source->records[load].statement.second) : NULL;
			long size;
			size_t storage;
			problem = "DMA must reference dedicated DS storage of at least 128 bytes";
			if (!buffer || objects[buffer->id - 1] || !equal(source->records[load].statement.opcode, "lxi") ||
			    !equal(source->records[load].statement.first, "d") ||
			    !storage_find(source, symbols, buffer, &storage, &size) || size < 128) goto done;
		}

		/* Conservative first slice: only the proved copier may write memory.
		 * Reject stack/pointer tricks and dynamically computed transfers too. */
		problem = "unproven memory write or pointer escape in FCB program";
		if (equal(stmt->opcode, "sta") || equal(stmt->opcode, "shld") || equal(stmt->opcode, "stax") ||
		    equal(stmt->opcode, "xthl") || equal(stmt->opcode, "pchl") || equal(stmt->opcode, "sphl") ||
		    equal(stmt->opcode, "push") || equal(stmt->opcode, "pop") ||
		    ((equal(stmt->opcode, "mov") || equal(stmt->opcode, "mvi") || equal(stmt->opcode, "inr") ||
		    equal(stmt->opcode, "dcr")) && equal(stmt->first, "m"))) goto done;
		if (equal(stmt->opcode, "lxi") && !equal(stmt->first, "sp")) {
			long address;
			if (!binding(symbols, stmt->second) &&
			    (!symbols_eval(symbols, stmt->second, -1, &address) || address >= 0x100)) {
				problem = "computed address cannot be relocated with FCB storage"; goto done;
			}
		}
		if (transfer(stmt)) {
			long address = symbol_value(symbols, stmt->first);
			if (address >= 0 && address != 0 && address != 5) { problem = "numeric transfer cannot be audited for FCB aliases"; goto done; }
			if (!equal(stmt->opcode, "jmp") && stmt->opcode[0] == 'c' && address_value(symbols, stmt->first) != 5 && !allowed[i]) {
				problem = "unrecognized subroutine may expose FCB storage"; goto done;
			}
		}
		/* Reject every unapproved occurrence of an FCB symbol, including
		 * arithmetic expressions and EQU aliases. Values are source offsets. */
		for (j = 0; j < symbols->count; ++j) if (objects[j]) {
			const symbol_t *object = &symbols->items[j];
			const char *operands[] = {stmt->first, stmt->second};
			for (size_t k = 0; k < 2; ++k) {
				long value;
				const symbol_t *direct = binding(symbols, operands[k]);
				if (allowed[i]) continue;
				if ((direct && direct->id == object->id) ||
				    (symbols_eval(symbols, operands[k], -1, &value) && object->source_address >= 0 &&
				    value >= object->source_address && value < object->source_address + object_size(source, symbols, object))) {
					problem = "FCB field access or address escape needs translation"; goto done;
				}
			}
		}
	}
	result = 1;
done:
	if (!result) fprintf(diagnostics, "xlt88: DOS FCB analysis: %s\n", problem);
	free(objects); free(allowed); free(regions);
	return result;
}
