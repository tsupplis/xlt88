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

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "trans.h"
#include "target.h"

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
	stream = fdopen(fd, "w");
	if (!stream) close(fd);
	return stream;
}

int
main(int argc, char *argv[])
{
	const backend_t *target = &msdos_backend;
	emitter_t emitter = nasm_emitter;
	target_program_t target_program = {0};
	source_program_t program = {0};
	symbol_table_t symbols = {0};
	const dialect_t *dialect = &intel_dialect;
	const char *input, *output, *state_in = NULL, *state_out = NULL;
	char *assembly_temp = NULL, *state_temp = NULL;
	FILE *fp = NULL, *out = NULL, *state = NULL;
	int arg = 1, normalized = 0, result = 1;

	while (arg < argc && argv[arg][0] == '-') {
		if (!strcmp(argv[arg], "--target=msdos")) target = &msdos_backend;
		else if (!strcmp(argv[arg], "--target=cpm86")) target = &cpm86_backend;
		else if (!strcmp(argv[arg], "--dialect=intel")) dialect = &intel_dialect;
		else if (!strcmp(argv[arg], "--emit=8080")) normalized = 1;
		else if (!strcmp(argv[arg], "--emit=nasm")) normalized = 0;
		else if (!strncmp(argv[arg], "--state-in=", 11) && argv[arg][11]) state_in = argv[arg] + 11;
		else if (!strncmp(argv[arg], "--state-out=", 12) && argv[arg][12]) state_out = argv[arg] + 12;
		else if (!strcmp(argv[arg], "--")) { ++arg; break; }
		else goto usage;
		++arg;
	}
	if (argc - arg != 2) goto usage;
	input = argv[arg];
	output = argv[arg + 1];
	if (same_path(input, output)) {
		fputs("xlt88: input and output must differ\n", stderr);
		return 1;
	}
	if (same_path(input, state_in) || same_path(input, state_out) ||
	    same_path(output, state_in) || same_path(output, state_out)) {
		fputs("xlt88: source, assembly and state paths must differ\n", stderr);
		return 1;
	}
	fp = fopen(input, "r");
	if (!fp) { perror(input); goto cleanup; }
	if (!dialect->read(fp, &program)) {
		fputs("xlt88: cannot read source program\n", stderr);
		goto cleanup;
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
	result = 0;
cleanup:
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
	fputs("usage: xlt88 [--target=msdos|cpm86] [--dialect=intel]"
	    " [--emit=nasm|8080] [--state-in=names.sym] [--state-out=names.sym]"
	    " infile.asm outfile.asm\n", stderr);
	return 1;
}
