/*
 * Copyright (c) 2021 Brian Callahan <bcallah@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "trans.h"
#include "asm86.h"
#include "masm.h"
#include "drasm86.h"
#include "target.h"
#include "comread.h"

/* Reject aliases before creating staging files. State input and output may
 * coincide; source and assembly must remain distinct from either state path. */
static char *
path_key(const char *path)
{
	const char *slash = strrchr(path, '/');
	const char *name = slash ? slash + 1 : path;
	size_t length = slash ? (size_t)(slash - path) : 1;
	char *directory = malloc(length + 2), *absolute, *key;
	if (!directory) return NULL;
	if (slash) {
		memcpy(directory, path, length);
		directory[length] = 0;
		if (!length) strcpy(directory, "/");
	} else strcpy(directory, ".");
	absolute = realpath(directory, NULL);
	free(directory);
	if (!absolute) return NULL;
	length = strlen(absolute) + strlen(name) + 2;
	key = malloc(length);
	if (key) snprintf(key, length, "%s/%s", absolute, name);
	free(absolute);
	return key;
}

static int
same_path(const char *a, const char *b)
{
	struct stat left, right;
	char *left_key, *right_key;
	int same;
	if (!a || !b) return 0;
	if (!strcmp(a, b) || (!stat(a, &left) && !stat(b, &right) &&
	    left.st_dev == right.st_dev && left.st_ino == right.st_ino)) return 1;
	left_key = path_key(a);
	right_key = path_key(b);
	same = left_key && right_key && !strcmp(left_key, right_key);
	free(left_key);
	free(right_key);
	return same;
}

static FILE *
stage_output(const char *path, char **temporary)
{
	const char *slash = strrchr(path, '/');
	size_t directory = slash ? (size_t)(slash - path + 1) : 0;
	size_t length = directory + sizeof("xlXXXXXX");
	int fd;
	FILE *stream;
	*temporary = malloc(length);
	if (!*temporary) return NULL;
	memcpy(*temporary, path, directory);
	memcpy(*temporary + directory, "xlXXXXXX", sizeof("xlXXXXXX"));
	fd = mkstemp(*temporary);
	if (fd < 0) return NULL;
	stream = fdopen(fd, "wb");
	if (!stream) close(fd);
	return stream;
}

static void
usage(FILE *out)
{
	fputs("usage: xlt88 [options] input output\n"
	    "\nExamples:\n"
	    "  xlt88 --target=cpm86 source.asm output.a86\n"
	    "  xlt88 --target=msdos source.asm output.asm\n"
	    "  xlt88 --dialect=mac --emit=asm80 source.mac review.asm\n"
	    "\nOptions:\n"
	    "  --target=msdos|cpm86                  Operating system (default: msdos)\n"
	    "  --dialect=asm80|m80|mac|rmac|com      Input reader (default: asm80, CP/M ASM)\n"
	    "  --emit=nasm|asm86|masm3|drasm86|asm80  Override output assembler\n"
	    "  --state-in=names.sym                 Read preferred symbol names\n"
	    "  --state-out=names.sym                Write symbol naming state\n"
	    "  --com-map=layout.map                 COM code/data/pointer annotations\n"
	    "  --help                              Show this help\n"
	    "\nOutput: .a86 selects ASM86; .asm and other extensions select NASM.\n"
	    "Explicit --emit takes precedence. Extensions never select the OS or input reader.\n"
	    "COM without --com-map discovers candidate code and writes output-stem.map.\n"
	    "Automatic COM recovery is experimental; native emission requires a reviewed map.\n"
	    "Use --emit=asm80 only to inspect the recovered source (exit 2, incomplete).\n"
	    "Exit codes: 0 translated, 1 error, 2 draft output written and review required.\n", out);
}

static char *
companion_path(const char *output, const char *suffix)
{
	const char *name = strrchr(output, '/');
	const char *dot = strrchr(name ? name + 1 : output, '.');
	size_t length = dot ? (size_t)(dot - output) : strlen(output);
	char *path = malloc(length + strlen(suffix) + 1);
	if (path) { memcpy(path, output, length); strcpy(path + length, suffix); }
	return path;
}

int
main(int argc, char *argv[])
{
	const backend_t *target = &msdos_backend;
	emitter_t emitter = nasm_emitter;
	target_program_t target_program = {0};
	source_program_t program = {0};
	symbol_table_t symbols = {0};
	const dialect_t *dialect = &asm80_dialect;
	const char *input, *output, *state_in = NULL, *state_out = NULL, *com_map = NULL;
	char *assembly_temp = NULL, *state_temp = NULL;
	char *map_path = NULL, *map_temp = NULL;
	FILE *fp = NULL, *out = NULL, *state = NULL, *map = NULL;
	int arg = 1, normalized = 0, result = 1, binary = 0;
	int explicit_emit = 0, discovery = 0;
	int (*render)(const target_program_t *, FILE *, FILE *) = NULL;

	while (arg < argc && argv[arg][0] == '-') {
		if (!strcmp(argv[arg], "--help")) { usage(stdout); return 0; }
		if (!strncmp(argv[arg], "--emit=", 7)) explicit_emit = 1;
		if (!strcmp(argv[arg], "--target=msdos")) target = &msdos_backend;
		else if (!strcmp(argv[arg], "--target=cpm86")) target = &cpm86_backend;
		else if (!strcmp(argv[arg], "--dialect=asm80")) { dialect = &asm80_dialect; binary = 0; }
		else if (!strcmp(argv[arg], "--dialect=m80")) { dialect = &m80_dialect; binary = 0; }
		else if (!strcmp(argv[arg], "--dialect=mac")) { dialect = &mac_dialect; binary = 0; }
		else if (!strcmp(argv[arg], "--dialect=rmac")) { dialect = &rmac_dialect; binary = 0; }
		else if (!strcmp(argv[arg], "--dialect=com")) binary = 1;
		else if (!strncmp(argv[arg], "--com-map=", 10) && argv[arg][10]) com_map = argv[arg] + 10;
		else if (!strcmp(argv[arg], "--emit=asm80")) normalized = 1;
		else if (!strcmp(argv[arg], "--emit=nasm")) { normalized = 0; render = NULL; }
		else if (!strcmp(argv[arg], "--emit=masm3")) { normalized = 0; render = masm_render; }
		else if (!strcmp(argv[arg], "--emit=drasm86")) { normalized = 0; render = drasm86_render; }
		else if (!strcmp(argv[arg], "--emit=asm86")) { normalized = 0; render = asm86_render; }
		else if (!strncmp(argv[arg], "--state-in=", 11) && argv[arg][11]) state_in = argv[arg] + 11;
		else if (!strncmp(argv[arg], "--state-out=", 12) && argv[arg][12]) state_out = argv[arg] + 12;
		else if (!strcmp(argv[arg], "--")) { ++arg; break; }
		else { fprintf(stderr, "xlt88: unknown option: %s\n", argv[arg]); goto usage; }
		++arg;
	}
	if (argc - arg != 2) goto usage;
	if (!binary && com_map) {
		fputs("xlt88: --com-map is only valid with --dialect=com\n", stderr);
		return 1;
	}
	input = argv[arg];
	output = argv[arg + 1];
	/* Output syntax is independent of the selected operating system. */
	if (!explicit_emit) {
		const char *extension = strrchr(output, '.');
		if (extension && !strcasecmp(extension, ".a86")) render = asm86_render;
	}
	if (same_path(input, output)) {
		fputs("xlt88: input and output must differ\n", stderr);
		return 1;
	}
	if (same_path(input, state_in) || same_path(input, state_out) ||
	    same_path(output, state_in) || same_path(output, state_out) ||
	    same_path(com_map, output) || same_path(com_map, state_out) ||
	    same_path(com_map, input) || same_path(com_map, state_in)) {
		fputs("xlt88: source, map, assembly and state paths must differ\n", stderr);
		return 1;
	}
	fp = fopen(input, "rb");
	if (!fp) { perror(input); goto cleanup; }
	discovery = binary && !com_map;
	if (discovery) {
		map_path = companion_path(output, ".map");
		if (!map_path) goto cleanup;
		if (same_path(map_path, input) || same_path(map_path, output) ||
		    same_path(map_path, state_in) || same_path(map_path, state_out)) {
			fputs("xlt88: generated map must differ from source, output and state paths\n", stderr);
			goto cleanup;
		}
		if (!access(map_path, F_OK)) {
			fprintf(stderr, "xlt88: map already exists: %s; use --com-map with the reviewed map or choose another output basename\n", map_path);
			goto cleanup;
		}
		map = stage_output(map_path, &map_temp);
		if (!map) { perror(map_path); goto cleanup; }
	} else if (binary) {
		map = fopen(com_map, "r");
		if (!map) { perror(com_map); goto cleanup; }
	}
	/* Both COM paths produce the same owned source as text readers. All
	 * symbol reconciliation, lowering and emission below are shared. */
	if (!(discovery ? com_analyze(fp, map, &program, stderr) :
	    binary ? com_read(fp, map, &program, stderr) : source_read(dialect, fp, input, &program))) {
		fputs("xlt88: cannot read source program\n", stderr);
		goto cleanup;
	}
	if (discovery) {
		if (fclose(map)) { map = NULL; perror(map_path); goto cleanup; }
		map = NULL;
		/* Retain the analysis even if later native lowering rejects a port
		 * requirement. The user can rerun with --emit=asm80 to inspect it. */
		if (rename(map_temp, map_path)) { perror(map_path); goto cleanup; }
		fprintf(stderr, "xlt88: draft map written to %s; classification requires review\n", map_path);
		if (!normalized) {
			fputs("xlt88: COM recovery is incomplete; native output was not written. --emit=asm80 provides an inspection listing only, not a working translation\n", stderr);
			goto cleanup;
		}
	}
	for (size_t i = 0; i < program.file_count; ++i) {
		if (same_path(program.files[i], output) || same_path(program.files[i], state_out)) {
			fputs("xlt88: output/state must not overwrite an included source\n", stderr);
			goto cleanup;
		}
	}
	if (!symbols_build(&program, &symbols, stderr)) goto cleanup;
	if (state_in) {
		state = fopen(state_in, "r");
		if (!state) { perror(state_in); goto cleanup; }
		if (!symbols_read(&symbols, state, stderr)) goto cleanup;
		fclose(state);
		state = NULL;
	}
	if (!normalized && lower_named(&program, &symbols, target, &target_program, stderr))
		goto cleanup;
	out = stage_output(output, &assembly_temp);
	if (!out) { perror(output); goto cleanup; }
	emitter.context = out;
	if (normalized) {
		if (!emit_named_8080(&program, &symbols, out, stderr)) goto cleanup;
	} else if (render) {
		if (!render(&target_program, out, stderr)) goto cleanup;
	} else if (!target_write(&target_program, &emitter)) goto cleanup;
	if (fclose(out)) { out = NULL; perror(output); goto cleanup; }
	out = NULL;
	if (state_out) {
		state = stage_output(state_out, &state_temp);
		if (!state) { perror(state_out); goto cleanup; }
		if (!symbols_write(&symbols, state)) goto cleanup;
		if (fclose(state)) { state = NULL; perror(state_out); goto cleanup; }
		state = NULL;
	}
	if (rename(assembly_temp, output)) { perror(output); goto cleanup; }
	if (state_out && rename(state_temp, state_out)) { perror(state_out); goto cleanup; }
	result = discovery ? 2 : 0;
	if (discovery)
		fprintf(stderr, "xlt88: draft output written to %s; review TODOs and %s before use (exit 2)\n", output, map_path);
cleanup:
	if (map) fclose(map);
	if (map_temp) { unlink(map_temp); free(map_temp); }
	free(map_path);
	if (fp) fclose(fp);
	if (out) fclose(out);
	if (state) fclose(state);
	if (assembly_temp) { unlink(assembly_temp); free(assembly_temp); }
	if (state_temp) { unlink(state_temp); free(state_temp); }
	symbols_destroy(&symbols);
	target_destroy(&target_program);
	source_destroy(&program);
	return result;
usage:
	usage(stderr);
	return 1;
}
