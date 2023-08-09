#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
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
#include "utils.h"

#if WINDOWS
#include <winsock2.h>
#pragma message("Building for windows")

int init_socket_layer(void)
{
	// Startup winsock.
	struct WSAData wsa_data = {0};
	int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
	if (result != 0) {
		printf("WSA Startup failed: %d.\n", result);
		return -1;
	}
	return 0;
}

void cleanup_socket_layer(void)
{
	WSACleanup();
}

int get_error(void)
{
	WSAGetLastError();
}

#elif UNIX
#pragma message("Building for MAC")
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int init_socket_layer(void)
{
  return 0;
}

void cleanup_socket_layer(void)
{
}

int get_error(void)
{
	return errno;
}

#else
#error "No build type selected"
#endif

static const unsigned short port = 8080;

void print(struct sockaddr_in *address)
{
	const unsigned ip = ntohl(address->sin_addr.s_addr);
	const unsigned byte4 = ip & 0xff000000;
	const unsigned byte3 = ip & 0x00ff0000;
	const unsigned byte2 = ip & 0x0000ff00;
	const unsigned byte1 = ip & 0x000000ff;
	const unsigned short port = ntohs(address->sin_port);
	printf("Contact: %u.%u.%u.%u:%u\n", byte4, byte3, byte2, byte1, port);
}

/*
 * Print blob which is len bytes long. If max_lines is -1, print the entire blob.
 * If max_lines is > 0, print at most that many lines of blob. A "line" contains
 * BLOB_LINE bytes of hex data and character data.
 */
#define BLOB_LINE (16)
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

void
print_request(struct request *r)
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

char *
header_find_value(struct request *r, const char *key)
{
	size_t i;
	for (i = 0; i < r->header_count; ++i) {
		if (strcmp(key, r->headers[i].key) == 0) {
			return r->headers[i].value;
		}
	}
	return NULL;
}

int parse_request_buffer(struct request *request)
{
	const char eol[] = "\r\n";
	const char index_page[] = "index.html";

	// Type, path and format are all on the first line.
	char *end_of_line = strstr(request->buffer, eol);
	if (!end_of_line) {
		// Uh... wut? We should have at least one EOL.
		fprintf(stderr, "Did not find EOL in header. Header:\n%s\n",
			request->buffer);
		return EINVAL;
	}
	// Put a null in to stop searches for spaces.
	*end_of_line = 0;
	// Find the space between GET\POST and the path.
	char *space = strstr(request->buffer, " ");
	if (!space) {
		fprintf(stderr, "Did not find GET\\POST to path space in header. Header: \"%s\"\n",
			request->buffer);
		return EINVAL;
	}

	// Set the space to null and then figure out the request type from the
	// string.
	*space = 0;
	if (strcmp(request->buffer, "GET") == 0) {
		request->type = GET;
	} else if (strcmp(request->buffer, "POST") == 0) {
		request->type = POST;
	} else {
		fprintf(stderr, "Unrecognized request type \"%s\"\n",
			request->buffer);
		return EINVAL;
	}

	// Now lets get the path.
	char *start = space + 1;
	space = strstr(start, " ");
	if (!space) {
		fprintf(stderr, "Failed to find path end: \"%s\"\n", start);
		return EINVAL;
	}
	*space = 0;

	// Handle some special cases for path. If it is just /, change it to
	// just load index.html. Easiest to do this before copying it into the
	// request.
	if (*start == '/') {
		start++;
	}
	/*
	 * If start is empty, default to index.html.
	 */
	if (strlen(start) == 0) {
		strncpy(request->path, index_page, STRMAX(request->path));
	} else {
		strncpy(request->path, start, STRMAX(request->path));
	}
	// If it is a directory (ends in /) append "index.html".
	size_t path_len = strlen(request->path);
	if (request->path[path_len] == '/') {
		strncat(request->path, index_page, strlen(request->path) -
			STRMAX(index_page));
	}

	// The rest of the line is the format.
	request->format = space + 1;

	// Now continue through the buffer looking for new lines. Each new
	// line delimits a http header (key: value). Then div it up into our
	// headers array.
	start = end_of_line + STRMAX(eol);
	while (start && (request->header_count < LEN(request->headers))) {
		request->headers[request->header_count].key = start;
		char *end = strstr(start, eol);
		if (end) {
			*end = 0;
			const char value_delimieter[] = ": ";
			char *value_start = strstr(start, value_delimieter);
			if (value_start) {
				*value_start = 0;
				request->headers[request->header_count].value =
					value_start + STRMAX(value_delimieter);
			} else {
				fprintf(stderr, "No value for \"%s\"\n",
					start);
			}
			request->header_count++;
		}
		start = end;
		if (start) {
			start += STRMAX(eol);
		}
	}

	printf("Found %lu headers.\n", request->header_count);

	return 0;
}

int parse_request(char *data, struct request *request)
{
	memset(request, 0, sizeof(*request));

	const char *header_separator = "\r\n\r\n";

	// Find the end of the header.
	char *end_of_header = strstr(data, header_separator);

	// If the header didn't end, copy the whole thing into the request.
	// Otherwise just copy the header.
	size_t amount;
	if (end_of_header) {
		amount = (size_t)(end_of_header - data);
	} else {
		amount = strlen(data);
	}

	// If the header is bigger than the request can store, refuse to
	// cooperate.
	if (amount >= STRMAX(request->buffer)) {
		fprintf(stderr, "Request header too big: %lu > %lu\n", amount,
			STRMAX(request->buffer));
		return ENOBUFS;
	}

	(void)strncpy(request->buffer, data, amount);
	memset(request->buffer + amount, 0, LEN(request->buffer) - amount);

	int result = parse_request_buffer(request);
	if (result) {
		fprintf(stderr, "Failed to parse request buffer %d.\n",
			result);
		return result;
	}

	// If its a post request set the parameters pointer, which will be
	// past the end of the header.
	if (request->type == POST) {
		char *param_start = end_of_header + strlen(header_separator);
		request->param_len = (size_t)((data + strlen(data)) -
			param_start);
		if (request->param_cap <= request->param_len) {
			free(request->parameters);
			request->parameters = calloc(request->param_len + 1,
				sizeof(*request->parameters));
			if (!request->parameters) {
				fprintf(stderr,
					"Failed to allocate new_params\n");
				return ENOBUFS;
			}
		}
		strncpy(request->parameters, param_start, request->param_len);
	} else {
		request->param_len = 0;
	}

	return 0;
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
 * Load the file the client requested and return it, otherwise return an error
 * page to the client.
 */
int handle_get_request(int client, struct request *request, struct pool *p)
{
	printf("Getting \"%s\"\n", request->path);
	if (strcmp(request->path, "asl.html") == 0) {
		printf("Dynamic URI\n");
		return asl_get(request, client);
	}
	printf("Regular URI\n");
	FILE *f = NULL;
	f = fopen(request->path, "r");
	if (!f) {
		fprintf(stderr, "%s not found.\n", request->path);
		return send_404(client);
	}
	int result = send_file(f, client, p);
	fclose(f);
	return result;
}

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

int handle_post_request(int client, struct request *r, struct pool *p,
	size_t bytes_received)
{
	char *value;
	long total_len;
	size_t bytes_needed;
	size_t size;
	char *new_buf;

	(void)client;
	(void)p;

	// Convert the content-length in the header to bytes.
	value = header_find_value(r, "Content-Length");
	if (!value) {
		fprintf(stderr, "Did not find Content-Length in header\n");
		print_request(r);
		return EINVAL;
	}
	total_len = strtol(value, NULL, 10);
	if ((errno == EINVAL) || (errno == ERANGE)) {
		fprintf(stderr, "Failed to convert %s to long. %u.\n", value,
			errno);
		return ERANGE;
	}
	if (total_len < 0) {
		fprintf(stderr, "Invalid content length %lu.\n", total_len);
		return EINVAL;
	}
	printf("content length is %ld\n", total_len);
	bytes_needed = (size_t)total_len;

	if (bytes_needed > GIGABYTE) {
		fprintf(stderr, "Request too big: %lu. Max: %d.\n",
			bytes_needed, GIGABYTE);
		return EINVAL;
	}
	printf("Have %lu bytes of content, Need to read in %lu more bytes\n",
		bytes_received, bytes_needed);
	
	if (r->param_len + bytes_needed < r->param_cap) {
		// Resize the buffer to hold all of the parameters.
		size = r->param_len + bytes_needed + 1;
		new_buf = calloc(size, sizeof(*new_buf));
		if (!new_buf) {
			fprintf(stderr, "Failed to allocate new buf.\n");
			return ENOBUFS;
		}
		strncpy(new_buf, r->parameters, size);
		free(r->parameters);
		r->parameters = new_buf;
		r->param_cap = size;
	}
	while (r->param_len < bytes_needed) {
		size_t space = bytes_needed - r->param_len;
		ssize_t in = read(client, r->parameters + r->param_len, space);
		if (in < 0) {
			fprintf(stderr, "Failed to read from client: %d.\n",
				errno);
			return errno;
		}
		r->param_len += (size_t)in;
		printf("Read %ld (%lu/%lu)\n", in, r->param_len, bytes_needed);
	}
	printf("Parameters read in. Total %lu\n", r->param_len);

	print_request(r);

	if (strcmp(r->path, "asl.html") == 0) {
		return asl_post(r, client);
	}
	printf("Don't know what to do with post to %s.\n", r->path);
	return 0;
}

int handle_client(int client, struct sockaddr_in *client_addr, struct pool *p)
{
	(void)client_addr;

	char buffer[8192] = {0};
	memset(buffer, 0, sizeof(buffer));
	size_t bytes_rxed = (size_t)recv(client, buffer, LEN(buffer) - 1, 0);
	if (bytes_rxed == (size_t)-1) {
		fprintf(stderr, "Failed to read from client: %d.\n",
			get_error());
		return -1;
	}

	struct request request;
	if (parse_request(buffer, &request) != 0) {
		fprintf(stderr,
			"Failed to parse client's request.\nBuffer was:\n%s\n",
			buffer);
		return -1;
	}
	int result;
	if (request.type == GET) {
		result = handle_get_request(client, &request, p);
	} else {
		result = handle_post_request(client, &request, p, bytes_rxed);
	}
	if (result != 0) {
		fprintf(stderr, "Failed to handle client %d\nBuffer was:\n%s\n",
			result, buffer);
	}
	return result;
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

int is_image(const char *file)
{
	char *file_types[] = {
		".png",
		".jpg",
		".jpeg",
	};
	int match;
	/*
	 * Loop through every character in the file type looking for the first
	 * character that doesn't match. If everything matched, return nonzero.
	 */
	const size_t name_len = strlen(file);

	for (size_t type = 0; type < LEN(file_types); ++type) {
		const size_t type_len = strlen(file_types[type]);
		if (name_len < type_len) {
			// Not a match.
			continue;
		}
		match = 1;
		const size_t ext_start = name_len - type_len;
		for (size_t i = 0; (i <= type_len) && (match); ++i) {
			if (file[ext_start + i] != file_types[type][i]) {
				match = 0;
			}
		}
		if (match) {
			printf("%s matched %s.\n", file, file_types[type]);
			return 1;
		}
	}
	return 0;
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
