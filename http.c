/*
 * Copyright (C) 2023 Joseph M Vrba
 *
 * Contains definitions and routines for HTTP requests.
 */
#include "http.h"

#include <assert.h>
#include <errno.h>
#include <linux/limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"

const char ok_header[] = "HTTP/1.1 200 OK";

int find_param(const struct request *r, const char *param_name,
	struct http_param *out)
{
	if (!out || !r || !param_name) return EINVAL;

	static_assert(SIZE_MAX > LONG_MAX);
	assert((size_t)r->header_count < LEN(r->headers));
	for (long i = 0; i < r->header_count; ++i) {
		if (str_cmp_cstr(&r->headers[i].key, param_name) == 0) {
			*out = r->headers[i];
			return 0;
		}
	}
	return ENOENT;
}

int header_find_value(struct request *r, const char *key, struct str *value)
{
	if (!r || !key || !value) return EINVAL;

	for (long i = 0; i < r->header_count; ++i) {
		if (str_cmp_cstr(&r->headers[i].key, key) == 0) {
			*value = r->headers[i].value;
			return 0;
		}
	}
	return ENOENT;
}

void print_request(struct request *r)
{
	const char *type_str = NULL;

	if (r->type == GET) {
		type_str = "GET";
	} else if (r->type == POST) {
		type_str = "POST";
	} else {
		type_str = "UNKNOWN";
	}
	printf("Request:\n"
		"Type: %s\n"
		"Path: ", type_str);
	str_print(stdout, &r->path);
	puts("\nFormat: ");
	str_print(stdout, &r->format);
	puts("\n");
	if (r->header_count > 0) {
		printf("Parameters:\n"
		       "-----------\n");
		for (long i = 0; i < r->header_count; ++i) {
			printf("%li: ", i);
			str_print(stdout, &r->headers[i].key);
			puts(":");
			str_print(stdout, &r->headers[i].value);
			puts("\n");
		}
		printf("-----------\n");
	}
}

int send_data(int client, const char *header, const char *contents,
	size_t content_len)
{
	static char buffer[MEGABYTE];
	int bytes;

	memset(buffer, 0, LEN(buffer));

	bytes = snprintf(buffer, STRMAX(buffer),
		"%s\r\nContent Length: %lu\r\n\r\n", header, content_len);
	if (bytes < 0) {
		fprintf(stderr, "Buf write failure. %d.\n", errno);
		return errno;
	}

	// Send header
	ssize_t bytes_sent = write(client, buffer, (size_t)bytes);
	if (bytes_sent == -1) {
		fprintf(stderr, "Failed to write buffer to client! %d\n",
			errno);
		return -1;
	}
	if (bytes_sent != bytes) {
		fprintf(stderr, "Only sent %ld of %d of header\n", bytes_sent,
			bytes);
		return -1;
	}
	printf("Sent %ld byte header and ", bytes_sent);

	// Send contents
	bytes_sent = write(client, contents, content_len);
	if (bytes_sent == -1) {
		perror("Failed to write buffer to client.");
		return errno;
	}
	if ((size_t)bytes_sent != content_len) {
		fprintf(stderr, "Only sent %ld of %ld of the content.\n",
			bytes_sent, content_len);
		return -1;
	}
	printf("%ld byte content.\n", bytes_sent);
	return 0;
}

int send_path(struct str *file_path, int client, struct pool *p)
{
	char path[PATH_MAX + 1] = {0};
	int err = str_copy_to_cstr(file_path, path, PATH_MAX);
	if (err) {
		fprintf(stderr, "Failed to copy path to c-string: %i\n", err);
		return err;
	}
	FILE *f = fopen(path, "r");
	if (!f) {
		fprintf(stderr, "Failed to open file \"%s\": %i\n", path,
			errno);
		return errno;
	}
	err = send_file(f, client, p);
	fclose(f);
	if (err)
		fprintf(stderr, "send_file failed for \"%s\": %i\n", path, err);
	return err;
}

int send_file(FILE *f, int client, struct pool *p)
{
	char *contents;
	long file_size;
	size_t chars_read;
	int result = 0;

	(void)p;

	if (fseek(f, 0, SEEK_END) != 0) {
		perror("Failed to seek to the end of the file.\n");
		return -1;
	}
	file_size = ftell(f);
	if (file_size < 0) {
		perror("Failed to read the file size of the file.\n");
		return -1;
	}
	if (fseek(f, 0, SEEK_SET) != 0) {
		perror("Failed to seek to beginning of the file.\n");
		return -1;
	}
	printf("File is %lu bytes.\n", file_size);

	long pool_cap = pool_get_remaining_capacity(p);
	if (pool_cap < file_size) {
		fprintf(stderr, "%s> No pool space. Needed %li have %li\n",
			__func__, file_size, pool_cap);
		return ENOBUFS;
	}
	long pool_pos = pool_get_position(p);
	assert(pool_pos != -1);
	contents = pool_alloc(p, file_size);
	if (!contents) {
		fprintf(stderr, "Failed to allocate buffer for file: %d.\n",
			errno);
		return errno;
	}

	// Note: don't return without resetting the pool now
	
	chars_read = fread((void*)contents, sizeof(*contents),
		(size_t)file_size, f);
	if (chars_read == (size_t)file_size) {
		result = send_data(client, ok_header, contents,
			(size_t)file_size);
	} else {
		printf("Failed to read in file: %lu of %ld\n", chars_read,
			file_size);
		result = errno;
	}

	if (result != 0) {
		fprintf(stderr, "Failed to send message.\n");
		result = errno;
	}
	pool_reset(p, pool_pos);

	return result;
}

int send_404(int client)
{
	static const char html[] = 
		"<html>"
		"  <head>"
		"    <title>Page Not Found</title>"
		"  </head>"
		"  <body>"
		"    <h1>Sorry that page doesn't exist</h1>"
		"  </body>"
		"</html>";
	static const char header[] = "HTTP/1.1 404 NOT FOUND";

	return send_data(client, header, html, STRMAX(html));
}

