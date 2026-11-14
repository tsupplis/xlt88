/* For 128-byte records the first DOS PSP FCB uses 005Ch..007Fh. Its name prefix matches
 * CP/M; the remainder belongs to native DOS after a translated file call.
 * Arbitrary computed memory has native target semantics. Keep identifiable
 * accesses to incompatible fields diagnostic without emulating CP/M memory. */
#include <string.h>
#include "fcbdef.h"
#include "fcbaddr.h"
#include "fcbapi.h"


static int
pair(const char *name)
{
	if (!strcmp(name, "b")) return 0;
	if (!strcmp(name, "d")) return 1;
	if (!strcmp(name, "h")) return 2;
	return -1;
}

static int
byte_register(const char *name)
{
	const char *found;
	if (!name[0] || name[1]) return -1;
	found = strchr("bcdehl", name[0]);
	return found ? (int)(found - "bcdehl") : -1;
}

static long
pair_value(const long *bytes, int reg)
{
	return reg >= 0 && bytes[reg * 2] >= 0 && bytes[reg * 2 + 1] >= 0 ?
	    bytes[reg * 2] * 256 + bytes[reg * 2 + 1] : -1;
}

static void
set_pair(long *bytes, int reg, long value)
{
	bytes[reg * 2] = value < 0 ? -1 : (value >> 8) & 255;
	bytes[reg * 2 + 1] = value < 0 ? -1 : value & 255;
}

static void
forget(long *bytes)
{
	for (int i = 0; i < 6; ++i) bytes[i] = -1;
}

static int
field(long address, int width)
{
	if (address < 0) return 0;
	for (int i = 0; i < width; ++i) {
		long byte = (address + i) & 65535;
		if (byte >= 0x68 && byte < 0x80) return 1;
	}
	return 0;
}

int
fcb_default_plan(const source_program_t *source, const symbol_table_t *symbols,
    const long *functions, unsigned char *calls, const char **problem)
{
	int defaults = 0, others = 0;
	long bytes[6];
	forget(bytes);
	for (size_t i = 0; i < source->count; ++i) {
		const statement_t *stmt = &source->records[i].statement;
		fcb_address_t address;
		long value;
		if (!symbols_eval(symbols, stmt->first, -1, &value) || value != 5 ||
		    !fcb_function(functions[i])) continue;
		if (!fcb_address_before(source, i, &address) ||
		    !symbols_eval(symbols, address.expression, -1, &value) || value != 0x5c) {
			others = 1;
			continue;
		}
		defaults = 1;
		calls[i] = 1;
	}
	if (!defaults) return 0;
	if (others) {
		*problem = "default FCB mixed with another file object needs separate storage analysis";
		return -1;
	}
	for (size_t i = 0; i < source->count; ++i) {
		const statement_t *stmt = &source->records[i].statement;
		const char *op = stmt->opcode;
		long value;
		int reg = pair(stmt->first);
		if (stmt->label[0]) forget(bytes);
		*problem = "explicit default-FCB metadata access needs field translation";
		if ((!strcmp(op, "lda") || !strcmp(op, "sta") ||
		    !strcmp(op, "lhld") || !strcmp(op, "shld")) &&
		    symbols_eval(symbols, stmt->first, -1, &value) &&
		    field(value, !strcmp(op, "lhld") || !strcmp(op, "shld") ? 2 : 1)) {
			/* Direct reads of R0..R2 retain their CP/M meaning for the
			 * native 128-byte-record FCB. Writes are still diagnosed. */
			if (!((!strcmp(op, "lda") && value >= 0x7d && value <= 0x7f) ||
			    (!strcmp(op, "lhld") && value >= 0x7d && value <= 0x7e))) return -1;
		}
		if ((!strcmp(stmt->first, "m") || !strcmp(stmt->second, "m")) &&
		    field(pair_value(bytes, 2), 1)) return -1;
		if ((!strcmp(op, "ldax") || !strcmp(op, "stax")) && reg >= 0 &&
		    field(pair_value(bytes, reg), 1)) return -1;
		if (!strcmp(op, "lxi") && reg >= 0) {
			set_pair(bytes, reg, symbols_eval(symbols, stmt->second, -1, &value) ? value & 65535 : -1);
			if (field(pair_value(bytes, reg), 1)) return -1;
		} else if ((!strcmp(op, "inx") || !strcmp(op, "dcx")) && reg >= 0) {
			value = pair_value(bytes, reg);
			set_pair(bytes, reg, value < 0 ? -1 :
			    (value + (!strcmp(op, "inx") ? 1 : -1)) & 65535);
		} else if (!strcmp(op, "xchg")) {
			for (int k = 0; k < 2; ++k) {
				value = bytes[2 + k]; bytes[2 + k] = bytes[4 + k]; bytes[4 + k] = value;
			}
		} else if (!strcmp(op, "dad")) {
			value = pair_value(bytes, reg);
			long hl = pair_value(bytes, 2);
			set_pair(bytes, 2, value < 0 || hl < 0 ? -1 : (value + hl) & 65535);
		} else if (!strcmp(op, "mov") || !strcmp(op, "mvi") ||
		    !strcmp(op, "inr") || !strcmp(op, "dcr")) {
			int dest = byte_register(stmt->first), origin = byte_register(stmt->second);
			if (dest >= 0) {
				if (!strcmp(op, "mov")) bytes[dest] = origin < 0 ? -1 : bytes[origin];
				else if (!strcmp(op, "mvi")) bytes[dest] =
				    symbols_eval(symbols, stmt->second, -1, &value) ? value & 255 : -1;
				else if (bytes[dest] >= 0) bytes[dest] =
				    (bytes[dest] + (!strcmp(op, "inr") ? 1 : -1)) & 255;
			}
		} else if (!strcmp(op, "lhld") || !strcmp(op, "xthl")) {
			set_pair(bytes, 2, -1);
		} else if (!strcmp(op, "pop") && reg >= 0) {
			set_pair(bytes, reg, -1);
		} else if (*op == 'j' || *op == 'c' || *op == 'r') {
			/* No interprocedural or join proof is asserted by this pass. */
			forget(bytes);
		}
	}
	return 1;
}
