#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <winsock2.h>
#include <stdio.h>
#include <iostream>

#define GIGABYTE (1000000000)

#define LEN(p) (sizeof(p) / sizeof(p[0]))

namespace server {

const unsigned short port = 8080;

enum request_type {
	GET,
	POST
};

struct pool {
	size_t offset;
	size_t cap;
	char *buffer;
};

int pool_init(struct pool *p, size_t desired_size)
{
	assert(p && (p->buffer == NULL));
	p->offset = 0;
	p->cap = desired_size;
	p->buffer = (char*)malloc(p->cap);

	if (!p->buffer) {
		return errno;
	}
	return 0;
}

void pool_free(struct pool *p)
{
	if (p && p->buffer) {
		free(p->buffer);
		p->buffer = NULL;
	}
	p->offset = p->cap = 0;
}

void *pool_alloc(struct pool *p, size_t byte_amount)
{
	size_t alignment = byte_amount % sizeof(void*);
	byte_amount += alignment;
	if ((p->offset + byte_amount) > p->cap) {
		return NULL;
	}
	assert(p->buffer);
	void *allocation = (void*)(p->buffer + p->offset);
	p->offset += byte_amount;
	return allocation;
}

#define pool_alloc_type(pool, type) (type*)pool_alloc(pool, sizeof(type))

#define pool_alloc_array(pool, type, count) (type*)pool_alloc(pool, sizeof(type) * count)

struct str {
	size_t len;
	char *data;
};

size_t str_find_substr(const struct str *target, const struct str *src)
{
	size_t src_char;
	size_t target_char;
	int found_location = -1;
	int found = 0;
	for (src_char = 0; src_char < src->len; ++src_char) {
		if (src->data[src_char] == target->data[0]) {
			assert(src_char < INT_MAX);
			found_location = (int)src_char;
			found = 1;
			for (target_char = 1; (target_char < target->len) && (src_char < src->len); ) {
				if (src->data[src_char] != target->data[target_char]) {
					found = 0;
					break;
				}
			}
			if (found) {
				return found_location;
			}
		}
	}
	return (size_t)-1;
}

int str_cmp(const struct str *a, const struct str *b)
{
	if (a->len > b->len) {
		return -1;
	} else if (a->len < b->len) {
		return 1;
	}
	return strncmp(a->data, b->data, a->len);
}

int str_cmp(const struct str *a, const char *b)
{
	size_t b_len = strlen(b);
	if (a->len > b_len) {
		return -1;
	} else if (a->len < b_len) {
		return 1;
	}
	return strncmp(a->data, b, a->len);
}

int str_alloc(struct str *s, const size_t len, struct pool *p)
{
	assert(s && p);
	if (len == 0) {
		return 0;
	}
	s->len = len;
	s->data = pool_alloc_array(p, char, s->len);
	if (s->data) {
		return 0;
	}
	return ENOMEM;
}

size_t str_cpy(const struct str *src, struct str *dest)
{
	size_t i = 0;
	size_t max = (src->len > dest->len) ? dest->len : src->len;
	for (i; i < max; ++i) {
		dest->data[i] = src->data[i];
	}
	return i;
}

struct str_list {
	struct str str;
	struct str_list *next;
};

struct str_list *split(const struct str *src, const struct str *delimiter, struct pool *p)
{
	struct str_list *root = NULL;
	struct str_list *end = NULL;
	struct str_list *next = NULL;
	int keep_going = 1;
	size_t last = 0;

	while (keep_going) {
		size_t eow = str_find_substr(src, delimiter);
		if (eow == -1) {
			// Create the new str_list.
			next = pool_alloc_type(p, struct str_list);
			assert(eow > last);
			size_t word_size = eow - last;
			int bytes_allocated = str_alloc(&next->str, word_size, p);
			if (bytes_allocated == ENOMEM) {
				return NULL;
			}
			memcpy(next->str.data, src->data + last, word_size);

			// Attach the str_list to the list of str_lists.
			if (end) {
				end->next = next;
			} else if (root) {
				end = root->next = next;
			} else {
				end = root = next;
			}
		}
	}
	return root;
}

struct request {
	request_type type;
	char path[1024];
	char format[1024];
};

int parse_request(const struct str *data, struct pool *p, struct request *request)
{
	char eol_chars[] = "\r\n";
	const str eol = { strlen(eol_chars), eol_chars };
	char space_chars[] = " ";
	const str space = { strlen(space_chars), space_chars };
	char get[] = "GET";

	struct str header;

	size_t first_eol = str_find_substr(&eol, data);
	if (first_eol == -1) {
		printf("Did not find header EOL.\n");
		return -1;
	}

	if (str_alloc(&header, first_eol, p) != 0) {
		printf("Failed to allocate header.\n");
		return -1;
	}

	size_t copies = str_cpy(data, &header);

	// Figure out what type of request this is.
	struct str_list *tokens = split(&header, &space, p);
	if (!tokens) {
		printf("Failed to break up header: \"%s\"\n", header.data);
		return -1;
	}
	if (str_cmp(&tokens->str, get)) {
		request->type = GET;
	}

	// Load the path.
	tokens = tokens->next;
	if (!tokens) {
		printf("Did not find path in header: \"%s\"\n", header.data);
		return -1;
	}
	size_t path_len = LEN(request->path) - 1;
	if (tokens->str.len < path_len) {
		path_len = tokens->str.len;
	}
	memcpy(request->path, tokens->str.data, path_len);

	// Load the request version.
	tokens = tokens->next;
	if (!tokens) {
		printf("Did not find the format in header: \"%s\"\n", header.data);
		return -1;
	}
	size_t format_len = LEN(request->format) - 1;
	if (format_len > tokens->str.len) {
		format_len = tokens->str.len;
	}
	memcpy(request->format, tokens->str.data, format_len);

	printf("Request type: \"%d\" path: \"%s\" format: \"%s\"\n",
		request->type, request->path, request->format);

	return -1;
}

int handle_client(SOCKET client, struct sockaddr_in *client_addr, struct pool *p)
{
	char buffer[4096] = {0};
	int bytes_rxed = recv(client, buffer, LEN(buffer) - 1, 0);
	if (bytes_rxed == SOCKET_ERROR) {
		printf("Failed to read from client: %d.\n", WSAGetLastError());
		return -1;
	}

	struct request request;
	struct str str = { strlen(buffer), buffer };
	parse_request(&str, p, &request);

	return -1;
}

int serve(SOCKET server_sock)
{
	struct pool p = {};
	int result = 0;
	int keep_running = 1;

	if (pool_init(&p, GIGABYTE) != 0) {
		printf("Failed to create memory pool: %d.\n", errno);
		return -1;
	}
	while (keep_running) {
		struct sockaddr_in client_addr = {0};
		int addr_len = sizeof(client_addr);
		printf("Waiting for connection...");
		SOCKET client = accept(server_sock, (SOCKADDR*)&client_addr, &addr_len);
		printf("contact detected.\n");
		if (client != INVALID_SOCKET) {
			assert(sizeof(client_addr) == addr_len);
			printf("Contact: %d.%d.%d.%d:%u\n",
				client_addr.sin_addr.S_un.S_un_b.s_b1,
				client_addr.sin_addr.S_un.S_un_b.s_b2,
				client_addr.sin_addr.S_un.S_un_b.s_b3,
				client_addr.sin_addr.S_un.S_un_b.s_b4,
				ntohs(client_addr.sin_port));
			result = handle_client(client, &client_addr, &p);
			closesocket(client);

			if (result != 0) {
				printf("Handling the client failed: %d\n", result);
				keep_running = 0;
			}
		} else {
			printf("Error accepting client connection: %d.\n", WSAGetLastError());
		}
	}
	pool_free(&p);
	return result;
}

} // End namespace server

int main()
{
	// Startup winsock.
	WSAData wsa_data = {0};
	int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
	if (result != 0) {
		printf("WSA Startup failed: %d.\n", result);
		return -1;
	}

	SOCKET server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server_sock != INVALID_SOCKET) {

		struct sockaddr_in address;
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = inet_addr("127.0.0.1");
		address.sin_port = htons(server::port);
		printf("Server will listen on port %hu.\n", server::port);

		result = bind(server_sock, (SOCKADDR*)&address, sizeof(address));
		if (0 == result) {
			result = listen(server_sock, SOMAXCONN);
			if (result != SOCKET_ERROR) {
				result = server::serve(server_sock);
			} else {
				printf("Server socket failed to listen: %d.\n", WSAGetLastError());
			}
		} else {
			printf("Failed to bind socket: %d", WSAGetLastError());
		}

		closesocket(server_sock);
	} else {
		printf("Could not create server socket: %d", WSAGetLastError());
		result = -1;
	}

	WSACleanup();
	return result;
}