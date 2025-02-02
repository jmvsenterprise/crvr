/**
 * Copyright (C) 2025 Joseph M Vrba
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "str.h"

struct quiz_question {
	char id[256];
	struct str question;
	struct str answer;
	unsigned long great_recalls;
	unsigned long good_recalls;
	unsigned long poort_recalls;
};

struct file_data {
	char *data;
	long len;
};

struct file_data config_data = {0};

static int read_entire_file(const char *path, struct file_data *dest);

int load_plugin(void)
{
	int error = 0;

	error = read_entire_file("text_quizzer.conf", &config_data);
	// Need to build the file if it doesn't exist, but wait to see what
	// message we get.
	return error;
}

static int
read_entire_file(const char *path, struct file_data *dest)
{
	FILE *f = NULL;
	long length = 0;
	size_t items_read = 0;
	int error = 0;

	if (!dest || dest->data) {
		return EINVAL;
	}

	f = fopen(path, "rb");
	if (!f) {
		printf("Failed to find file: %s: %s\n", path, strerror(errno));
		error = errno ? errno : EINVAL;
		goto cleanup;
	}

	fseek(f, 0, SEEK_END);
	length = ftell(f);

	rewind(f);

	dest->len = length;
	dest->data = malloc((unsigned long)length);
	if (!dest->data) {
		printf("Failed to allocate block for file %s: %s\n", path,
			strerror(errno));
		error = errno? errno: ENOMEM;
		goto cleanup;
	}

	items_read = fread(dest->data, 1, (unsigned long)dest->len, f);
	if ((long)items_read != dest->len) {
		printf("Failed to read in %s. Read %lu not %li: %s.\n", path,
			items_read, dest->len, strerror(errno));
		error = errno? errno: -1;
	}
cleanup:
	if (dest->data) {
		free(dest->data);
	}
	if (f) {
		fclose(f);
	}
	return error;
}
