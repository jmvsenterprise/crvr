#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

const unsigned short port = 8080;

enum request_type {
	GET,
	POST
};

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
	size_t alignment = sizeof(void*) - byte_amount % sizeof(void*);
	/*
	printf("Allocating %lu alignment=%lu actual=%lu\n", byte_amount,
		alignment, byte_amount + alignment);
	*/
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
			src_char++;
			for (target_char = 1; (target_char < target->len) && (src_char < src->len); ) {
				if (src->data[src_char] != target->data[target_char]) {
					found = 0;
					break;
				}
				src_char++;
				target_char++;
			}
			if (found) {
				return (size_t)found_location;
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

int str_cmp_chars(const struct str *a, const char *b)
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
	for (; i < max; ++i) {
		dest->data[i] = src->data[i];
	}
	return i;
}

void str_print(const struct str *str)
{
	if (str) {
		printf("{%lu, \"%s\"}", str->len, str->data);
	} else {
		printf("{NULL}");
	}
}

struct str_list {
	struct str str;
	struct str_list *next;
};

struct str_list *split(const struct str *src, const struct str *delimiter,
	struct pool *p)
{
	struct str moving_src = *src;
	struct str_list *root = NULL;
	struct str_list *end = NULL;
	struct str_list *next = NULL;
	size_t eow = 0;
	size_t word_size;

	printf("%s: Looking for \"%s\" in \"%s\".\n", __func__, delimiter->data,
		src->data);
	while (eow != (size_t)-1) {
		eow = str_find_substr(delimiter, &moving_src);
		// Create the new str_list.
		next = pool_alloc_type(p, struct str_list);
		if (!next) {
			printf("Failed to allocate str_list.\n");
			return NULL;
		}
		if (eow == (size_t)-1) {
			word_size = moving_src.len + 1;
			printf("No delimiter found. Final word: %s.\n",
				moving_src.data);
		} else {
			word_size = eow;
			printf("Delimiter found at %lu: %s\n", eow,
				moving_src.data + eow);
		}
		printf("Word size is %lu.\n", word_size);
		if (str_alloc(&next->str, word_size, p) == ENOMEM) {
			printf("Failed to allocate next's str.\n");
			return NULL;
		}
		memcpy(next->str.data, moving_src.data, next->str.len);
		printf("next is {%lu, \"%s\"}.\n", next->str.len,
			next->str.data);

		// Advance our src pointer now that we've copied the data.
		moving_src.data += word_size + 1;
		moving_src.len -= word_size + 1;
		printf("Remaining src: \"%s\". Len=%lu.\n", moving_src.data,
			moving_src.len);

		// Attach the str_list to the list of str_lists.
		if (end) {
			end->next = next;
			printf("Updated end->next: {%lu, %s}\n",
				end->next->str.len, end->next->str.data);
			end = next;
		} else if (root) {
			end = root->next = next;
			printf("Updated root->next: {%lu, %s}\n",
				root->next->str.len, root->next->str.data);
		} else {
			end = root = next;
			printf("Updated end: {%lu, %s}\n", end->str.len,
				end->str.data);
		}
	}
	printf("%s: Done splitting. Final list:\n", __func__);
	next = root;
	while (next) {
		str_print(&next->str);
		printf("\n");
		next = next->next;
	}
	return root;
}

struct request {
	enum request_type type;
	char path[1024];
	char format[1024];
};

int parse_request(const struct str *data, struct pool *p, struct request *request)
{
	char eol_chars[] = "\r\n";
	const struct str eol = { strlen(eol_chars), eol_chars };
	char space_chars[] = " ";
	const struct str space = { strlen(space_chars), space_chars };
	char get[] = "GET";

	struct str header;

	size_t first_eol = str_find_substr(&eol, data);
	if (first_eol == (size_t)-1) {
		printf("Did not find header EOL.\n");
		return -1;
	}

	if (str_alloc(&header, first_eol, p) != 0) {
		printf("Failed to allocate header.\n");
		return -1;
	}

	(void)str_cpy(data, &header);

	// Figure out what type of request this is.
	struct str_list *tokens = split(&header, &space, p);
	if (!tokens) {
		printf("Failed to break up header: \"%s\"\n", header.data);
		return -1;
	}
	if (str_cmp_chars(&tokens->str, get)) {
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
		printf("Did not find the format in header: \"%s\"\n",
			header.data);
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

int handle_client(int client, struct sockaddr_in *client_addr, struct pool *p)
{
	(void)client_addr;

	char buffer[4096] = {0};
	size_t bytes_rxed = (size_t)recv(client, buffer, LEN(buffer) - 1, 0);
	if (bytes_rxed == (size_t)-1) {
		printf("Failed to read from client: %d.\n", get_error());
		return -1;
	}

	struct request request;
	struct str str = { strlen(buffer), buffer };
	parse_request(&str, p, &request);

	return -1;
}

int serve(int server_sock)
{
	struct pool p = {0};
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
				printf("Handling the client failed: %d\n", result);
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
