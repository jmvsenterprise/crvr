/*
 * Copyright (C) 2023 Joseph M Vrba
 *
 * Contains definitions and routines for HTTP requests.
 */
#include "http.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"

const char ok_header[] = "HTTP/1.1 200 OK";

int find_param(struct param out[static 1], struct request r[static 1],
	const char *param_name)
{
	*out = (struct param){0};

	if (!param_name)
		return EINVAL;
	char *param = strstr(r->parameters, param_name);
	if (!param) {
		printf("Did not find %s in parameters.\n", param_name);
		return EINVAL;
	}

	printf("param: \"%s\"\n", param);

	size_t param_name_len = strlen(param_name);
	(void)strncpy(out->name, param_name, param_name_len);

	param += param_name_len;
	printf("after name param: \"%s\"\n", param);
	if ('=' != *param) {
		printf("Param is not '=': \"%c\" (0x%hhx)\n", *param, *param);
		return EINVAL;
	}
	param += sizeof((char)'=');
	printf("after = param: \"%s\"\n", param);

	/*
	 * Look for a \r\n or \0. If either one is encountered that's the end
	 * of the parameter value.
	 */
	size_t value_end = 0;
	printf("Checking param: ");
	for (; param[value_end]; ++value_end) {
		printf("'%c', ", param[value_end]);
		if (('\n' == param[value_end]) && (value_end > 0) &&
				(param[value_end - 1] == '\r')) {
			value_end -= (sizeof((char)'\r') + sizeof((char)'\n'));
			break;
		}
	}
	printf("\n");

	for (size_t i = 0; i < value_end; ++i) {
		out->value[i] = param[i];
	}
	printf("param %s:%s.\n", out->name, out->value);

	return 0;
}

char *header_find_value(struct request *r, const char *key)
{
	size_t i;
	for (i = 0; i < r->header_count; ++i) {
		if (strcmp(key, r->headers[i].key) == 0) {
			return r->headers[i].value;
		}
	}
	return NULL;
}

void print_request(struct request *r)
{
	const char *type_str = NULL;
	size_t i;

	if (r->type == GET) {
		type_str = "GET";
	} else if (r->type == POST) {
		type_str = "POST";
	} else {
		type_str = "UNKNOWN";
	}
	printf("Request:\n"
		"Type: %s\n"
		"Path: %s\n"
		"Format: %s\n"
		"Headers:\n",
		type_str, r->path, r->format);
	for (i = 0; i < r->header_count; ++i) {
		printf("\t%s: %s\n", r->headers[i].key, r->headers[i].value);
	}
	if (r->parameters) {
		printf("Parameters:\n"
		       "-----------\n");
		print_blob(r->parameters, r->param_len, 20);	
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
	contents = calloc((size_t)file_size, sizeof(*contents));
	if (!contents) {
		fprintf(stderr, "Failed to allocate buffer for file: %d.\n",
			errno);
		return errno;
	} else {
		chars_read = fread((void*)contents, sizeof(*contents),
			(size_t)file_size, f);
		if (chars_read != (size_t)file_size) {
			printf("Failed to read in file: %lu of %ld\n",
				chars_read, file_size);
			result = errno;
		}

		result = send_data(client, ok_header, contents,
			(size_t)file_size);
		if (result < 0) {
			fprintf(stderr, "Failed to send message.\n");
			result = errno;
		}
		free(contents);
	}
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

