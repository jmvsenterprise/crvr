/**
 * Copyright (C) 2025 Joseph M Vrba
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http.h"
#include "str.h"

#define CONFIG_FILE "text_quizzer.conf"

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

static int create_default_config(void);
static int read_entire_file(const char *path, struct file_data *dest);

int load_plugin(void)
{
	int error = 0;

	error = read_entire_file(CONFIG_FILE, &config_data);
	// Need to build the file if it doesn't exist, but wait to see what
	// message we get.
	if (error == ENOENT) {
		printf("%s: Config file not found, creating default\n",
			__FILE__);
		error = create_default_config();
		if (error) {
			printf("%s: Failed to create default config: %i\n",
				__FILE__, error);
			return error;
		}
		error = read_entire_file(CONFIG_FILE, &config_data);
	}

	if (error) {
		printf("%s: Failed to read default config: %i\n",
			__FILE__, error);
		return error;
	}

	printf("%s: Default config:\n\"%s\"\n", __FILE__, config_data.data);

	return error;
}

int
unload_plugin(void)
{
	return 0;
}

int
handle_post(struct request *r, int client)
{
	(void)r;
	(void)client;
	return 0;
}

int
handle_get(struct request *r, int client)
{
	(void)r;
	(void)client;
	return 0;
}

static int
create_default_config(void)
{
	FILE *f = fopen(CONFIG_FILE, "wb");
	if (!f) {
		printf("Failed to create %s: %i-%s\n", CONFIG_FILE, errno,
			strerror(errno));
		return errno? errno: EPERM;
	}

	if (fprintf(f, "%s", "[quizzes]\nexample.txt") < 0) {
		printf("Failed to write default value to %s: %i-%s\n",
			CONFIG_FILE, errno, strerror(errno));
		return errno? errno: EIO;
	}

	fclose(f);

	return 0;
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

	printf("file is length %li\n", length);

	rewind(f);

	dest->len = length;
	// +1 for null in case someone wants to print the contents.
	dest->data = malloc((unsigned long)length + 1);
	if (!dest->data) {
		printf("Failed to allocate block for file %s: %s\n", path,
			strerror(errno));
		error = errno? errno: ENOMEM;
		goto cleanup;
	}
	memset(dest->data, 0, length + 1);

	items_read = fread(dest->data, 1, (unsigned long)dest->len, f);
	if ((long)items_read != dest->len) {
		printf("Failed to read in %s. Read %lu not %li: %s.\n", path,
			items_read, dest->len, strerror(errno));
		error = errno? errno: -1;
	}

	printf("Read %li of %lu items from %s.\n", items_read, dest->len,
		path);

	goto good_cleanup;

cleanup:
	if (dest->data) {
		free(dest->data);
		dest->data = 0;
		dest->len = 0;
	}
good_cleanup:
	if (f) {
		fclose(f);
	}
	return error;
}
