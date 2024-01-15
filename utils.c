/*
 * Copyright (C) 2023 Joseph M Vrba
 *
 * This file contains definitions of utility routines.
 */
#include "utils.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The number of bytes to show on a line in print_blob.
#define BLOB_LINE (16)

void debug(const char *format, ...)
{
	va_list args;
	va_start(args, format);

	vprintf(format, args);

	va_end(args);
}

int load_file(const char *file_name, char *buffer, const size_t buf_len,
	size_t *bytes_loaded)
{
	assert(bytes_loaded && file_name && buffer && buf_len > 0);

	int result = 0;
	size_t bytes_read;
	FILE *f;

	f = fopen(file_name, "r");
	if (!f) {
		fprintf(stderr, "Failed to open %s: %d\n", file_name, errno);
		return errno;
	}
	*bytes_loaded = 0;
	while (!feof(f) && !ferror(f)) {
		bytes_read = fread(buffer + *bytes_loaded, sizeof(*buffer),
			buf_len - *bytes_loaded, f);
		*bytes_loaded += bytes_read;
	}
	if (ferror(f)) {
		result = errno;
		fprintf(stderr, "Error reading %s: %d.\n", file_name, result);
	}
	fclose(f);
	return result;
}

/*
 * Print blob which is len bytes long. If max_lines is -1, print the entire blob.
 * If max_lines is > 0, print at most that many lines of blob. A "line" contains
 * BLOB_LINE bytes of hex data and character data.
 */
void print_blob(const char *blob, const size_t len, int max_lines)
{
	size_t count = 0;
	unsigned line_offset = 0;
	char line[BLOB_LINE];
	for (size_t i = 0; i < len; ++i) {
		line[count++] = blob[i];
		if (count == LEN(line)) {
			if (max_lines != -1) {
				if (max_lines == 0)
					return;
				else
					max_lines--;
			}
			printf("0x%08x: ", line_offset);
			for (size_t c = 0; c < LEN(line); ++c) {
				printf("%2.2hhx ", line[c]);
			}
			for (size_t c = 0; c < LEN(line); ++c) {
				if (isalnum(line[c]) || ispunct(line[c])) {
					printf("%c", line[c]);
				} else {
					printf(".");
				}
			}
			printf("\n");
			line_offset += LEN(line);
			count = 0;

		}
	}
	if (count > 0) {
		printf("0x%08x: ", line_offset);
		for (size_t c = 0; c < count; ++c) {
			printf("%2.2hhx ", line[c]);
		}
		for (size_t c = 0; c < count; ++c) {
			if (isalnum(line[c]) || ispunct(line[c])) {
				printf("%c", line[c]);
			} else {
				printf(".");
			}
		}
	}
	printf("\n");
}


/*
 * Print the value of a variable to the buffer.
 *
 * buf - The buffer to replace the variable in.
 * buf_len - The current length of valid data in the buffer.
 * buf_cap - The max size the buffer can be. buf_len should be < buf_cap.
 * var_name - The name of the variable being replaced.
 * format - The format of the string to replace the variable with.
 * ... - Additional arguments that go into the format.
 *
 * Returns 0 if the variable was successfully replaced in the buffer.
 */
int print_var_to(char *buf, size_t *buf_len, const size_t buf_cap,
	const char *var_name, const char *format, ...)
{
	va_list args;
	int result;
	char var_str[KILOBYTE] = {0};
	size_t var_len;
	size_t buf_space;
	size_t var_name_len = strlen(var_name);

	va_start(args, format);

	result = vsnprintf(var_str, STRMAX(var_str), format, args);

	va_end(args);

	if (result < 0) {
		fprintf(stderr, "Failed to pack variable.\n");
		return ENOBUFS;
	}
	result = 0;

	/* Compute how much space is needed. */
	var_len = strlen(var_str);
	buf_space = buf_cap - *buf_len;
	if (var_len > buf_space) {
		fprintf(stderr, "Need %lu more bytes in buffer.\n",
			var_len - buf_space);
		return ENOBUFS;
	}

	/* Move the rest of the buffer down and insert the variable. */
	printf("var_len: %lu var_name_len: %lu *buf_len: %lu.\n", var_len,
		var_name_len, *buf_len);
	memmove(buf, buf - var_len + var_name_len, *buf_len + var_len);
	*buf_len += var_len - var_name_len;

	/* Write the variable in. */
	memcpy(buf, var_str, var_len);

	return result;
}
