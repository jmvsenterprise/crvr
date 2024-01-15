/*
 * Copyright (C) 2023 Joseph M Vrba
 *
 * This file contains the main routine and helper routines for the crvr
 * c[e]rv[e]r program.
 */
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <linux/limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "asl.h"
#include "pool.h"
#include "socket_layer.h"
#include "str.h"
#include "utils.h"

// Default port for the webserver
static const unsigned short port = 8080;

/*
 * Prints the address in a sockaddr_in.
 */
void print(struct sockaddr_in *address)
{
	const unsigned ip = ntohl(address->sin_addr.s_addr);
	const unsigned char byte4 = (unsigned char)((ip & 0xff000000) >> 24);
	const unsigned char byte3 = (unsigned char)((ip & 0x00ff0000) >> 16);
	const unsigned char byte2 = (unsigned char)((ip & 0x0000ff00) >> 8);
	const unsigned char byte1 = (unsigned char)(ip & 0x000000ff);
	const unsigned short port = ntohs(address->sin_port);
	printf("Contact: %hhu.%hhu.%hhu.%hhu:%hu\n", byte4, byte3, byte2, byte1,
		port);
}

static int parse_into_param(struct str *line, struct http_param *param)
{
	static const struct str separator = STR(":");

	if (!line || !param) return EINVAL;
	const long sep_loc = str_find_substr(line, &separator);
	if (sep_loc == -1) return EPROTO;
	param->key = (struct str){line->s, sep_loc};
	param->value = (struct str){line->s + sep_loc, line->len - sep_loc};
	return 0;
}

static int add_param_to_request(struct request *r, struct http_param *param)
{
	if (!r || !param) return EINVAL;
	assert(LEN(r->params) <= LONG_MAX);
	if (r->param_count >= (long)LEN(r->params)) return ENOBUFS;
	r->params[r->param_count] = *param;
	r->param_count++;
	return 0;
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

	while (rest_of_header.len > 0) {
		struct str line = rest_of_header;
		long eol = str_find_substr(&line, &newline);
		if (eol != -1) line.len = eol;
		rest_of_header.s += line.len;
		rest_of_header.len -= line.len;

		error = parse_into_param(&line, &param);
		if (error) return error;
		error = add_param_to_request(r, &param);
	}

	printf("Found %lu headers.\n", r->param_count);
	return 0;
}

/*
 * Parse the buffer in the request. Determine if it is a POST or GET request
 * and parse its parameters storing the data in the request itself.
 */
int parse_request_buffer(struct request *request, struct pool *p)
{
	static const struct str eol = STR("\r\n");
	static const struct str index_page = STR("index.html");
	static const struct str space = STR(" ");
	static const struct str slash_only = STR("/");

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
	struct str header = {req_buf.s, end_of_line};

	// Find the space between GET\POST and the path.
	long req_type_end = str_find_substr(&header, &space);
	if (req_type_end == -1) {
		fputs("Did not find GET\\POST to path space in header. Header: \"",
			stderr);
		str_print(stderr, &header);
		fputs("\"\n", stderr);
		return EINVAL;
	}
	struct str req_type = {header.s, req_type_end};

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
	struct str path = {header.s + req_type.len, header.len - req_type.len};
	long path_end = str_find_substr(&path, &space);
	if (path_end == -1) {
		fprintf(stderr, "Failed to find path end: \"");
		str_print(stderr, &header);
		fprintf(stderr, "\"\n");
		return EINVAL;
	}
	path.len = path_end;

	// The rest of the line is the format.
	request->format = (struct str){
		path.s + path.len,
		header.len - req_type.len - path.len
	};

	// Handle some special cases for path. If it is just /, change it to
	// just load index.html. Easiest to do this before copying it into the
	// request.

	if (str_cmp(&path, &slash_only) == 0) {
		path = index_page;
	}

	// If it is a directory (ends in /) append "index.html".
	if (path.s[path.len] == '/') {
		struct str actual_path = {0};
		long space_needed = path.len + index_page.len;
		int error = alloc_str(p, space_needed, &actual_path);
		if (error) {
			fprintf(stderr, "Failed to allocate actual path: %i.\n",
				error);
			return EINVAL;
		}
		assert(actual_path.len == space_needed);
		assert(actual_path.len >= path.len + index_page.len);
		static_assert(sizeof(size_t) >= sizeof(path.len));
		static_assert(SIZE_MAX > LONG_MAX);
		(void)memcpy(actual_path.s, path.s, (size_t)path.len);
		(void)strncat(actual_path.s + actual_path.len, index_page.s,
			(size_t)index_page.len);

		path = actual_path;
	}

	const struct str rest_of_header = {header.s + header.len,
	       req_buf.len - header.len};
	parse_header_options(rest_of_header, request);
	return 0;
}

int parse_request(char *data, long data_len, struct request *request,
	struct pool *p)
{
	if (!data || !request || (data_len <= 0)) return EINVAL;

	memset(request, 0, sizeof(*request));

	const struct str header_separator = STR("\r\n\r\n");

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
	if (end_of_header != -1) return ENOBUFS;

	int err = parse_request_buffer(request, p);
	if (err) {
		fprintf(stderr, "Failed to parse request buffer %d.\n", err);
		return err;
	}

	return 0;
}


/*
 * Load the file the client requested and return it, otherwise return an error
 * page to the client.
 */
int handle_get_request(int client, struct request *request, struct pool *p)
{
	static const struct str ASL_PAGE = STR("asl.html");
	printf("Getting \"");
	str_print(stdout, &request->path);
	printf("\"\n");

	if (str_cmp(&request->path, &ASL_PAGE) == 0) {
		printf("Dynamic URI\n");
		return asl_get(request, client);
	}
	FILE *f = NULL;
	char file_path[PATH_MAX] = {0};

	int err = str_copy_to_cstr(&request->path, file_path, PATH_MAX);
	if (err) return err;

	f = fopen(file_path, "r");
	if (!f) {
		fprintf(stderr, "\"%s\" not found.\n", file_path);
		err = send_404(client);
	} else {
		err = send_file(f, client, p);
		fclose(f);
	}
	return err;
}

int handle_post_request(int client, struct request *r, struct pool *p,
	size_t bytes_received)
{
	long total_len = 0;
	long bytes_needed = 0;

	(void)client;
	(void)p;

	// Convert the content-length in the header to bytes.
	struct str content_len = {0};
	int err = header_find_value(r, "Content-Length", &content_len);
	if (err) {
		fprintf(stderr, "Did not find Content-Length in header\n");
		print_request(r);
		return EINVAL;
	}
	err = str_to_long(&content_len, &total_len, 10);
	if (err) {
		fputs("Failed to convert \"", stderr);
		str_print(stderr, &content_len);
		fprintf(stderr, "\" to long. err=%u.\n", err);
		return err;
	}
	if (total_len < 0) {
		fprintf(stderr, "Invalid content length %lu.\n", total_len);
		return EINVAL;
	}
	printf("content length is %ld\n", total_len);
	bytes_needed = total_len;

	if (bytes_needed > pool_get_remaining_capacity(p)) {
		fprintf(stderr, "Request too big: %lu. Max: %li.\n",
			bytes_needed, pool_get_remaining_capacity(p));
		return ENOBUFS;
	}
	printf("Have %lu bytes of content, Need to read in %lu more bytes\n",
		bytes_received, bytes_needed);

	alloc_str(p, bytes_needed, &r->post_params_buffer);
	long bytes_read = 0;
	while (bytes_read < bytes_needed) {
		// Need to update space below if this isn't the case.
		assert(SIZE_MAX > LONG_MAX);
		size_t space = (size_t)(bytes_needed - bytes_read);
		ssize_t in = read(client, r->post_params_buffer.s + bytes_read,
			space);
		if (in < 0) {
			fprintf(stderr, "Failed to read from client: %d.\n",
				errno);
			return errno;
		}
		bytes_read += in;
		printf("Read %ld (%lu/%lu)\n", in, bytes_read, bytes_needed);
	}

	print_request(r);

	if (str_cmp_cstr(&r->path, "asl.html") == 0) return asl_post(r, client);

	printf("Don't know what to do with post to \"");
	str_print(stdout, &r->path);
	printf("\"\n");

	return 0;
}

int handle_client(int client, struct sockaddr_in *client_addr, struct pool *p)
{
	(void)client_addr;

	char buffer[8192] = {0};
	memset(buffer, 0, sizeof(buffer));
	ssize_t recv_result = -1;
	do {
		recv_result = recv(client, buffer, LEN(buffer) - 1, 0);
	} while ((recv_result == -1) && (errno == EAGAIN));
	
	size_t bytes_rxed = (size_t)recv_result;
	if (bytes_rxed == (size_t)-1) {
		fprintf(stderr, "Failed to read from client: %d.\n",
			get_error());
		return -1;
	}

	struct request request;
	const long start = pool_get_position(p);
	if (parse_request(buffer, LEN(buffer), &request, p) != 0) {
		fprintf(stderr,
			"Failed to parse client's request.\nBuffer was:\n%s\n",
			buffer);
		return -1;
	}
	int err = 0;
	if (request.type == GET) {
		err = handle_get_request(client, &request, p);
	} else {
		err = handle_post_request(client, &request, p, bytes_rxed);
	}
	if (err != 0) {
		fprintf(stderr, "Failed to handle client %d\nBuffer was:\n%s\n",
			err, buffer);
	}
	pool_reset(p, start);
	return err;
}

int serve(int server_sock)
{
	struct pool p = {};
	int result = 0;
	int keep_running = 1;

	if (pool_init(&p, GIGABYTE) != 0) {
		printf("Failed to create memory pool: %d.\n", errno);
		return -1;
	}
	while (keep_running) {
		struct sockaddr_in client_addr;
		memset(&client_addr, 0, sizeof(client_addr));
		unsigned addr_len = sizeof(client_addr);
		printf("Waiting for connection...");
		int client = accept(server_sock,
			(struct sockaddr*)&client_addr, &addr_len);
		printf("contact detected.\n");
		if (client != -1) {
			assert(sizeof(client_addr) == addr_len);
			print(&client_addr);
			result = handle_client(client, &client_addr, &p);
			close(client);

			if (result != 0) {
				printf("Handling the client failed: %d\n",
					result);
			}
		} else {
			printf("Error accepting client connection: %d.\n",
				get_error());
		}
	}
	pool_free(&p);
	return result;
}

int main()
{
	int result = 0;

	// Load the ASL app
	if (asl_init() != 0) {
		fprintf(stderr, "Failed to initialize ASL\n");
		return -1;
	}

	// Load the server up
	if (init_socket_layer() != 0) {
		printf("Failed to initialize the socket layer\n");
		return get_error();
	}
	int server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server_sock != -1) {
		struct sockaddr_in address;
		address.sin_family = AF_INET;
		//address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		address.sin_addr.s_addr = htonl(INADDR_ANY);
		address.sin_port = htons(port);
		printf("Server will listen on port %hu.\n", port);

		result = bind(server_sock, (struct sockaddr*)&address,
			sizeof(address));
		if (0 == result) {
			result = listen(server_sock, SOMAXCONN);
			if (result != -1) {
				result = serve(server_sock);
			} else {
				printf("Server socket failed to listen: %d.\n",
					get_error());
				result = -1;
			}
		} else {
			printf("Failed to bind socket: %d\n", get_error());
			result = -1;
		}

		close(server_sock);
	} else {
		printf("Could not create server socket: %d\n", get_error());
		result = -1;
	}

	cleanup_socket_layer();
	return result;
}
