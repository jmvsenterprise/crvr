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
static const struct str s_index_page = STR("index.html");
static const struct str s_line_end = STR("\r\n");

// Local functions
static int add_param_to_request(struct request *r, struct http_param *param);
static int add_post_param(struct request *r, struct http_param *param);
static int parse_into_param(struct str *line, struct http_param *param);
static int parse_request_buffer(struct request *request, struct pool *p);
/**
 * @brief Handle some special cases for the path.
 *
 * This function handles redirecting a request if its path meets certain
 * conditions. Like / should redirect to index.html, /blah should redirect to
 * just blah, etc.
 *
 * @param[in] path - The path to check.
 *
 * @return Returns 0 unless an error occurs.
 */
static int modify_path(struct request *r, struct pool *p);

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

int find_post_param(const struct request *r, const char *param_name,
	struct http_param *out)
{
	if (!r || !param_name || !out) return EINVAL;

	static_assert(SIZE_MAX > LONG_MAX);
	assert((size_t)r->post_param_count < LEN(r->post_params));
	for (long i = 0; i < r->post_param_count; ++i) {
		if (str_cmp_cstr(&r->post_params[i].key, param_name) == 0) {
			*out = r->post_params[i];
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

/**
 * @brief Parses the header after the request line.
 *
 * Now continue through the buffer looking for new lines. Each new line
 * delimits a http header (key: value). Then div it up into our headers array.
 *
 * @param[in] rest_of_header - the remaining header after the request line.
 * @param[in,out] r - The request to populate with data.
 *
 * @return Returns 0 if the header is successfully parsed. Otherwise returns an
 *         error code.
 */
static int parse_header_options(struct str rest_of_header, struct request *r)
{
	int error = 0;
	static const struct str newline = STR("\r\n");
	struct http_param param = {0};

	if (!r) {
		return EINVAL;
	}

	while ((rest_of_header.len > 0) && (r->header_count <
		(long)LEN(r->headers)))
	{
		long eol = str_find_substr(&rest_of_header, &newline);
		struct str line;
		int err = str_get_substr(&rest_of_header, 0, eol, &line);
		if (err) {
			fputs("Failed to get line from \"", stderr);
			str_print(stderr, &rest_of_header);
			fprintf(stderr, "\": %i\n", err);
			return err;
		}

		rest_of_header.s += line.len + newline.len;
		rest_of_header.len -= line.len + newline.len;

		error = parse_into_param(&line, &param);
		if (error) {
			fputs("Failed to parse param \"", stderr);
			str_print(stderr, &line);
			fprintf(stderr, "\": %i\n", error);
			return error;
		}
		error = add_param_to_request(r, &param);
	}

	printf("Found %lu headers.\n", r->header_count);
	return 0;
}


int parse_request(char *data, long data_len, struct request *request,
	struct pool *p)
{
	if (!data || !request || (data_len <= 0)) return EINVAL;

	memset(request, 0, sizeof(*request));

	static const struct str header_separator = STR("\r\n\r\n");

	const size_t data_str_len = strlen(data);
	if (data_str_len < (size_t)data_len) {
		request->buffer.len = (long)data_str_len;
	} else {
	       	request->buffer.len = data_len;
	}
	request->buffer.s = data;

	// Find the end of the header.
	long end_of_header = str_find_substr(&request->buffer,
		&header_separator);

	// If we didn't find the end of the header, refuse to parse it. Probably
	// need a bigger buffer.
	if (end_of_header == -1) {
		DEBUG("No end to header found\n");
		return ENOBUFS;
	}

	int err = parse_request_buffer(request, p);
	if (err) {
		fprintf(stderr, "Failed to parse request buffer %d.\n", err);
		return err;
	}

	return 0;
}

/*
 * Parse the buffer in the request. Determine if it is a POST or GET request
 * and parse its parameters storing the data in the request itself.
 */
static int parse_request_buffer(struct request *request, struct pool *p)
{
	static const struct str eol = STR("\r\n");
	static const struct str header_end = STR("\r\n\r\n");
	static const struct str space = STR(" ");

	// Type, path and format are all on the first line.
	struct str req_buf = request->buffer;

	long end_of_line = str_find_substr(&req_buf, &eol);
	if (end_of_line == -1) {
		// Uh... wut? We should have at least one EOL.
		fputs("Did not find EOL in header. Header:\n", stderr);
		str_print(stderr, &request->buffer);
		fputs("\n", stderr);
		return EINVAL;
	}
	struct str header;
	int err = str_get_substr(&req_buf, 0, end_of_line, &header);
	if (err) {
		fprintf(stderr, "Failed to substr the header: %i\n", err);
		return err;
	}

	// Find the space between GET\POST and the path.
	long req_type_end = str_find_substr(&header, &space);
	if (req_type_end == -1) {
		fputs("Did not find GET\\POST to path space in header. Header: \"",
			stderr);
		str_print(stderr, &header);
		fputs("\"\n", stderr);
		return EINVAL;
	}
	struct str req_type;
	err = str_get_substr(&header, 0, req_type_end, &req_type);
	if (err) {
		fprintf(stderr, "Failed to substr request type: %i\n", err);
		return err;
	}

	// Figure out the request type from the string.
	if (str_cmp_cstr(&req_type, "GET") == 0) {
		request->type = GET;
	} else if (str_cmp_cstr(&req_type, "POST") == 0) {
		request->type = POST;
	} else {
		fputs("Unrecognized request type \"", stderr);
		str_print(stderr, &req_type);
		fputs("\"\n", stderr);
		return EINVAL;
	}

	// Now lets get the path.
	err = str_get_substr(&header, req_type_end + space.len, EOSTR,
		&request->path);
	if (err) {
		fprintf(stderr, "Failed to substr path: %i\n", err);
		return err;
	}
	long path_end = str_find_substr(&request->path, &space);
	if (path_end == -1) {
		fprintf(stderr, "Failed to find path end: \"");
		str_print(stderr, &header);
		fprintf(stderr, "\"\n");
		return EINVAL;
	}
	request->path.len = path_end;

	// The rest of the line is the format.
	err = str_get_substr(&header, req_type.len + space.len +
		request->path.len + space.len, EOSTR, &request->format);
	if (err) {
		fprintf(stderr, "Failed to substr format: %i\n", err);
		return err;
	}

	err = modify_path(request, p);
	if (err) {
		fprintf(stderr, "%s> failed to modify path: %i\n", __func__,
			err);
		return err;
	}

	long eoh = str_find_substr(&req_buf, &header_end);
	struct str rest_of_header;
	err = str_get_substr(&req_buf, header.len + eol.len, eoh,
		&rest_of_header);
	if (err) {
		fprintf(stderr, "Failed to substr rest of header: %i\n", err);
		return err;
	}
	parse_header_options(rest_of_header, request);
	return 0;
}

int parse_post_parameters(struct request *r)
{
	if (!r) return EINVAL;

	r->post_param_count = 0;
	if (r->post_params_buffer.len <= 0) {
		printf("%s> No post params\n", __func__);
		return 0;
	}

	struct str buf = r->post_params_buffer;
	// Need to fix the cast below if this static assert fails.
	static_assert((size_t)LONG_MAX > LEN(r->post_params));
	for (; (buf.len > 0) && (r->post_param_count <
		(long)LEN(r->post_params));)
	{
		long eol = str_find_substr(&buf, &s_line_end);
		struct str line = {0};
		if (eol != -1) {
			int err = str_get_substr(&buf, 0, eol, &line);
			if (err) {
				fprintf(stderr, "%s> Failed to get substr. eol=%li, buf=\"",
					__func__, eol);
				str_print(stderr, &buf);
				fprintf(stderr, "\": %i\n", err);
				return err;
			}
		} else {
			line = buf;
		}
		struct http_param param = {0};
		int err = parse_into_param(&line, &param);
		if (err) {
			fprintf(stderr, "%s> Failed to parse param from \"",
				__func__);
			str_print(stderr, &line);
			fprintf(stderr, "\": %i\n", err);
			return err;
		}
		err = add_post_param(r, &param);
		if (err) {
			fprintf(stderr, "%s> add post param failed: %i\n",
				__func__, err);
			return err;
		}
		buf.len -= line.len;
		buf.s = buf.s + line.len + s_line_end.len;
	}
	return 0;
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

static int add_post_param(struct request *r, struct http_param *param)
{
	if (!r || !param) return EINVAL;

	// If this assert doesn't work, need to fix the cast below it.
	static_assert((size_t)LONG_MAX > LEN(r->post_params));
	if (r->post_param_count < (long)LEN(r->post_params)) {
		r->post_params[r->post_param_count] = *param;
		r->post_param_count++;
		return 0;
	}
	return ENOBUFS;
}

static int parse_into_param(struct str *line, struct http_param *param)
{
	static const struct str separator = STR(": ");

	if (!line || !param) return EINVAL;
	if (line->len == 0) return EINVAL;
	const long sep_location = str_find_substr(line, &separator);
	if (sep_location == -1) {
		fputs("Failed to find separator for \"", stderr);
		str_print(stderr, line);
		fputs("\"\n", stderr);
		return EPROTO;
	}
	int err = str_get_substr(line, 0, sep_location, &param->key);
	if (err) {
		fprintf(stderr, "Failed to get key from [0, %li] in \"",
			sep_location);
		str_print(stderr, line);
		fprintf(stderr, "\": %i\n", err);
		return err;
	}
	const long value_start = sep_location + separator.len;
	err = str_get_substr(line, value_start, EOSTR, &param->value);
	if (err) {
		fprintf(stderr, "Failed to get value from [%li, -1] in \"",
			value_start);
		str_print(stderr, line);
		fprintf(stderr, "\": %i\n", err);
		return err;
	}
	return 0;
}

static int add_param_to_request(struct request *r, struct http_param *param)
{
	if (!r || !param) return EINVAL;
	assert(LEN(r->headers) <= LONG_MAX);
	if (r->header_count >= (long)LEN(r->headers)) {
		DEBUG("invalid param count");
		return ENOBUFS;
	}
	r->headers[r->header_count] = *param;
	r->header_count++;
	return 0;
}

static int modify_path(struct request *r, struct pool *p)
{
	if (!r) return EINVAL;

	// Remove the leading /. Prevent stinkers sending ///////...
	while ((r->path.len > 0) && (r->path.s[0] == '/')) {
		r->path.len--;
		r->path.s++;
	}

	// If the path is empty, change it to load index.html.
	if (r->path.len == 0) {
		r->path = s_index_page;
	}

	// If the path is a directory (ends in /) append "index.html".
	if (r->path.s[r->path.len] == '/') {
		struct str actual_path = {0};
		// Note... could just have a PATH_MAX buffer in the request
		// instead of bothering with dynamic allocation...
		long space_needed = r->path.len + s_index_page.len;
		int error = str_alloc(p, space_needed, &actual_path);
		if (error) {
			fprintf(stderr, "Failed to allocate actual path: %i.\n",
				error);
			return EINVAL;
		}
		assert(actual_path.len == space_needed);
		assert(actual_path.len >= r->path.len + s_index_page.len);
		static_assert(sizeof(size_t) >= sizeof(r->path.len));
		static_assert(SIZE_MAX > LONG_MAX);
		(void)memcpy(actual_path.s, r->path.s,
			(size_t)r->path.len);
		(void)strncat(actual_path.s + actual_path.len, s_index_page.s,
			(size_t)s_index_page.len);

		r->path = actual_path;
	}
	return 0;
}


