/*
 * Copyright (C) 2023 Joseph M Vrba
 *
 * This file contains definitions of utility routines.
 */
#include "utils.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
