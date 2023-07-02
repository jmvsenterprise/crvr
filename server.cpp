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
	size_t offset = 0;
	size_t cap = 0;
	char *buffer = nullptr;
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
	std::string path;
	std::string format;
};

std::ostream& operator<<(std::ostream& os, const request& r)
{
	return os << "{type:" << r.type << ", path:" << r.path << ", format:" <<
		r.format << "}";
}

int parse_request(const std::string& data, request& request)
{
	size_t end_of_line = data.find_first_of("\r\n");
	const std::string header(data.substr(0, end_of_line));

	std::cout << "header: \"" << header << "\"\n";

	size_t end_of_type = header.find_first_of(" ");
	const std::string type{header.substr(0, end_of_type)};

	if (end_of_type == header.npos) {
		std::cerr << "Request header missing path: " << header << "\n";
		return -1;
	}
	std::cout << "end of type: " << end_of_type << "\n";

	size_t end_of_path = header.find_first_of(" ", end_of_type + 1);
	if (end_of_path == header.npos) {
		std::cerr << "Request header missing format: " << header <<
			"\n";
		return -1;
	}
	std::cout << "end of path: " << end_of_path << "\n";
	std::string path{header.substr(end_of_type + 1, end_of_path -
		(end_of_type + 1))};
	// If the path has a leading /, get rid of it.
	std::cout << "path: \"" << path << "\"\n";
	if (path[0] == '/') {
		path.erase(path.begin());
		std::cout << "path updated: \"" << path << "\"\n";
	}
	const std::string format{header.substr(end_of_path + 1)};

	if (type == "GET") {
		request.type = GET;
	} else if (type == "POST") {
		request.type = POST;
	} else {
		std::cerr << "Unrecognized request type: " << type << "\n";
		return -1;
	}
	request.path = path;
	request.format = format;

	std::cout << "New request: " << request << "\n";

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
	static const std::string html{"<html><head><title>Page Not Found</title></head><body><h1>Sorry that page doesn't exist</h1></body></html>"};
	static const std::string header{"HTTP/1.1 404 NOT FOUND"};

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

	std::string contents(static_cast<size_t>(file_size), '\0');

	size_t chars_read = fread(contents.data(),
		sizeof(decltype(contents)::value_type), contents.length(),
		f);
	if (chars_read != contents.length()) {
		printf("Failed to read in the entire file: %lu of %ld\n",
			chars_read, contents.length());
		return -1;
	}

	static const std::string header{"HTTP/1.1 200 OK"};
	return send(client, header, contents);
}

/*
 * Load the file the client requested and return it, otherwise return an error
 * page to the client.
 */
int handle_request(int client, struct request *request, struct pool *p)
{
	std::filesystem::path path{request->path};
	if ((path == "/") || (path.empty())) {
		path = "index.html";
		std::cout << "Requested root (" << path << ")\n";
	}
	if (std::filesystem::is_directory(path)) {
		path += "index.html";
		std::cout << "Directory requested.\n";
	}
	std::cout << "Getting " << path << "\n";
	FILE *f = nullptr;
	f = fopen(path.c_str(), "r");
	if (!f) {
		std::cerr << "File " << path << " not found.\n";
		return send_404(client);
	}
	int result = send_file(f, client, p);
	fclose(f);
	return result;
}

int handle_client(int client, struct sockaddr_in *client_addr, struct pool *p)
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
	std::string data{buffer};
	if (parse_request(buffer, request) != 0) {
		printf("Failed to parse the client's request.\n");
		return -1;
	}
	if ((request.path == "?") || (request.type == POST)) {
		std::cout << "POST? requested. Dumping full buffer:\n" <<
			buffer << "\n";
	}
	return handle_request(client, &request, p);
}

int serve(int server_sock)
{
	struct pool p;
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
