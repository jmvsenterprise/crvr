#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <filesystem>
#include <iostream>

extern "C" {
#include "pool.h"
}

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

#define GIGABYTE (1000000000)

#define LEN(p) (sizeof(p) / sizeof(p[0]))
#define STRMAX(p) (LEN(p) - 1)

const unsigned short port = 8080;

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

enum request_type {
	GET,
	POST
};

/*
 * NOTE: I could have the request store the entire header and split up the parts
 * by inserting \0s into it.
 */
struct request {
	enum request_type type;
	char path[256];
	char *format;
	size_t header_count;
	struct header {
		char *key;
		char *value;
	} headers[20];
	char buffer[1024];
	char *parameters;
};

std::ostream& operator<<(std::ostream& os, const enum request_type type)
{
	switch (type) {
	case GET:
		return os << "GET";
	case POST:
		return os << "POST";
	default:
		return os << "Unrecognized type " <<
			static_cast<unsigned>(type);
	}
}

std::ostream& operator<<(std::ostream& os, const struct request& r)
{
	return os << "{type:" << r.type << ", path:" << r.path
		<< ", format:" << r.format << "}";
}

int parse_request_buffer(struct request *request)
{
	const char eol[] = "\r\n";

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
	strncpy(request->path, start, STRMAX(request->path));
	// If it is a directory (ends in /) append an index.html.
	size_t path_len = strlen(request->path);
	if (request->path[path_len] == '/') {
		strlcat(request->path, "index.html", STRMAX(request->path));
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
		request->parameters = end_of_header + strlen(header_separator);
	} else {
		request->parameters = NULL;
	}

	return 0;
}

int send(int client, const std::string& header, const std::string& contents)
{
	static std::string buffer;
	buffer.clear();
	buffer += header;
	buffer += "Content Length: ";
	buffer += std::to_string(contents.length());
	buffer += "\r\n\r\n";
	buffer += contents;

	ssize_t bytes_sent = write(client, buffer.c_str(), buffer.length());
	if (bytes_sent == -1) {
		std::cerr << "Failed to write buffer to client! " << errno <<
			"\n";
		return -1;
	}
	if (static_cast<size_t>(bytes_sent) != buffer.length()) {
		std::cerr << "Failed to send buffer to client!\nOnly wrote " <<
			bytes_sent << " of " << buffer.length() << " bytes.\n";
		return -1;
	}
	return 0;
}

int send_404(int client)
{
	static const std::string html(
		"<html>"
		"  <head>"
		"    <title>Page Not Found</title>"
		"  </head>"
		"  <body>"
		"    <h1>Sorry that page doesn't exist</h1>"
		"  </body>"
		"</html>"
	);
	static const std::string header(
		"HTTP/1.1 404 NOT FOUND"
	);

	return send(client, header, html);
}

int send_file(FILE *f, int client, struct pool *p)
{
	(void)p;

	if (fseek(f, 0, SEEK_END) != 0) {
		perror("Failed to seek to the end of the file.\n");
		return -1;
	}
	long file_size = ftell(f);
	if (file_size == -1) {
		perror("Failed to read the file size of the file.\n");
		return -1;
	}
	if (fseek(f, 0, SEEK_SET) != 0) {
		perror("Failed to seek to beginning of the file.\n");
		return -1;
	}

	std::string contents(static_cast<size_t>(file_size),
		'\0');

	size_t chars_read = fread(
		reinterpret_cast<void*>(contents.data()),
		sizeof(decltype(contents)::value_type),
		contents.length(), f);
	if (chars_read != contents.length()) {
		printf("Failed to read in the entire file: %lu of %ld\n",
			chars_read, contents.length());
		return -1;
	}

	static const std::string header("HTTP/1.1 200 OK");
	return send(client, header, contents);
}

/*
 * Load the file the client requested and return it, otherwise return an error
 * page to the client.
 */
int handle_get_request(int client, struct request *request, struct pool *p)
{
	std::cout << "Getting " << request->path << "\n";
	FILE *f = nullptr;
	f = fopen(request->path, "r");
	if (!f) {
		std::cerr << "File " << request->path << " not found.\n";
		return send_404(client);
	}
	int result = send_file(f, client, p);
	fclose(f);
	return result;
}

int handle_post_request(int client, struct request *request, struct pool *p)
{
	(void)client;
	(void)p;
	printf("parameters length: %lu. parameters:\n=====\n%s\n=====\n",
		strlen(request->parameters), request->parameters);
	return EINVAL;
}

int handle_client(int client, struct sockaddr_in *client_addr, pool& p)
{
	(void)client_addr;

	char buffer[8192] = {0};
	memset(buffer, 0, sizeof(buffer));
	size_t bytes_rxed = (size_t)recv(client, buffer, LEN(buffer) - 1, 0);
	if (bytes_rxed == (size_t)-1) {
		printf("Failed to read from client: %d.\n", get_error());
		return -1;
	}

	struct request request;
	if (parse_request(buffer, &request) != 0) {
		std::cerr << "Failed to parse the client's request.\n" <<
			"Buffer was:\n" << buffer << "\n";
		return -1;
	}
	int result;
	if (request.type == GET) {
		result = handle_get_request(client, &request, &p);
	} else {
		result = handle_post_request(client, &request, &p);
	}
	if (result != 0) {
		std::cerr << "Failed to handle client: " << result <<
			"\nBuffer was:\n" << buffer << "\n";
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
			result = handle_client(client, &client_addr, p);
			close(client);

			if (result != 0) {
				printf("Handling the client failed: %d\n",
					result);
				keep_running = 0;
			}
		} else {
			printf("Error accepting client connection: %d.\n", get_error());
		}
	}
	pool_free(&p);
	return result;
}

int main()
{
	int result = 0;
	if (init_socket_layer() != 0) {
		printf("Failed to initialize the socket layer\n");
	}
	int server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server_sock != -1) {

		struct sockaddr_in address;
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		address.sin_port = htons(port);
		printf("Server will listen on port %hu.\n", port);

		result = bind(server_sock, (struct sockaddr*)&address,
			sizeof(address));
		if (0 == result) {
			result = listen(server_sock, SOMAXCONN);
			if (result != -1) {
				result = serve(server_sock);
			} else {
				printf("Server socket failed to listen: %d.\n", get_error());
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
