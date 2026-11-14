#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "dialect.h"

static source_program_t
read_text(const dialect_t *reader, const char *text, size_t length)
{
	source_program_t source = {0};
	FILE *stream = tmpfile();
	assert(stream);
	assert(fwrite(text, 1, length, stream) == length);
	rewind(stream);
	assert(reader->read(stream, &source));
	fclose(stream);
	return source;
}

int
main(void)
{
	const char text[] = "  start: mvi a,1\r\n    db 'a,b;!',0\r\tend\x1a ignored";
	const char m80[] = ".8080\nTITLE Someone's test\nSUBTTL test\nPAGE\n.LIST\n.XLIST\nstart: MVI A,1\nJMP start\nEND\n";
	const char *bad[] = {".Z80\n", "foo MACRO\n", "IF 1\n", "INCLUDE x.asm\n", "CSEG\n", "PUBLIC foo\n", ".8080 1\n", "foo:: nop\n"};
	source_program_t source = read_text(&intel_dialect, text, sizeof(text)-1);
	size_t i;
	assert(source.count == 3);
	for (i = 0; i < source.count; ++i) assert(!source.records[i].problem && source.records[i].line == i+1);
	assert(!strcmp(source.records[0].statement.label, "start"));
	assert(!strcmp(source.records[0].statement.opcode, "mvi"));
	assert(!strcmp(source.records[1].statement.first, "'a,b;!',0"));
	source_destroy(&source);
	source = read_text(&m80_dialect, m80, sizeof(m80)-1);
	assert(source.count == 9);
	for (i = 0; i < source.count; ++i) assert(!source.records[i].problem);
	for (i = 0; i < 6; ++i) assert(source.records[i].statement.comment[0] == ';');
	assert(!strcmp(source.records[7].statement.opcode, "jmp"));
	source_destroy(&source);
	for (i = 0; i < sizeof(bad)/sizeof(bad[0]); ++i) {
		source = read_text(&m80_dialect, bad[i], strlen(bad[i]));
		{
			size_t j; int found = 0;
			for (j = 0; j < source.count; ++j) found |= source.records[j].problem != NULL;
			assert(found);
		}
		source_destroy(&source);
	}
	{
		char longline[258];
		memset(longline, 'x', sizeof(longline)); longline[0] = ';';
		for (i = 254; i <= 255; ++i) {
			source = read_text(&intel_dialect, longline, i);
			assert(source.count == 1);
			assert((source.records[0].problem != NULL) == (i == 255));
			source_destroy(&source);
		}
	}
	{
		const char nul[] = " nop\0hidden\n end\n";
		source = read_text(&intel_dialect, nul, sizeof(nul)-1);
		assert(source.count == 2 && source.records[0].problem && source.records[1].line == 2);
		source_destroy(&source);
	}
	puts("Reader syntax, M80 subset, line endings, CP/M EOF and rejection checks passed");
	return 0;
}
