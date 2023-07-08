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

#include "pool.h"

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

struct Request {
	enum class Type {
		Get,
		Post
	};

	Type type;
	std::filesystem::path path;
	std::string format;
	std::string parameters;
};

std::ostream& operator<<(std::ostream& os, const Request::Type type)
{
	switch (type) {
	case Request::Type::Get:
		return os << "GET";
	case Request::Type::Post:
		return os << "POST";
	default:
		return os << "Unrecognized type " <<
			static_cast<unsigned>(type);
	}
}

std::ostream& operator<<(std::ostream& os, const Request& r)
{
	return os << "{type:" << r.type << ", path:" << r.path
		<< ", format:" << r.format << "}";
}

int parse_request(char *data, Request& request)
{
	std::string buffer(data);

	size_t end_of_header = buffer.find_first_of("\r\n");
	std::string header;
	if (end_of_header == buffer.npos) {
		header = buffer;
	} else {
		header = buffer.substr(0, end_of_header);
	}

	// Break up the header.
	size_t end_of_type = header.find_first_of(" ");
	std::string type = header.substr(0, end_of_type);

	size_t end_of_path = header.find_first_of(" ", end_of_type + 1);
	std::string path = header.substr(end_of_type + 1,
		(end_of_path - end_of_type - 1));

	request.format = header.substr(end_of_path + 1);	

	if (type == "GET") {
		request.type = Request::Type::Get;
	} else if (type == "POST") {
		request.type = Request::Type::Post;
	} else {
		std::cerr << "Unrecognized request type \"" << type << "\"\n";
		return EINVAL;
	}

	if (path.empty() || (path == "/")) {
		path = "index.html";
	} else if (path[0] == '/') {
		path.erase(path.begin());
	}
	request.path = path;
	if (std::filesystem::is_directory(request.path)) {
		request.path += "index.html";
		std::cout << "Directory requested.\n";
	}

	if (request.format != "HTTP/1.1") {
		std::cerr << "Unrecognized format \"" << request.format <<
			"\"\n";
		return EINVAL;
	}

	if (request.type == Request::Type::Post) {
		// Find the parameters.
		static const std::string param_delimiter = "\r\n\r\n";
		size_t param_start = buffer.find_first_of(param_delimiter,
			end_of_header + 1);
		request.parameters = buffer.substr(param_start +
			param_delimiter.length());
	}

	std::cout << "Request is " << request << "\n";

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

int send_file(FILE *f, int client, pool& p)
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
int handle_get_request(int client, Request& request, pool& p)
{
	std::cout << "Getting " << request.path << "\n";
	FILE *f = nullptr;
	f = fopen(request.path.c_str(), "r");
	if (!f) {
		std::cerr << "File " << request.path << " not found.\n";
		return send_404(client);
	}
	int result = send_file(f, client, p);
	fclose(f);
	return result;
}

int handle_post_request([[maybe_unused]] int client, Request& request,
	[[maybe_unused]] pool& p)
{
	std::cout << "Posted parameters: \"" << request.parameters << "\"\n";
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

	Request request;
	if (parse_request(buffer, request) != 0) {
		std::cerr << "Failed to parse the client's request.\n" <<
			"Buffer was:\n" << buffer << "\n";
		return -1;
	}
	int result;
	if (request.type == Request::Type::Get) {
		result = handle_get_request(client, request, p);
	} else {
		result = handle_post_request(client, request, p);
	}
	if (result != 0) {
		std::cerr << "Failed to handle client: " << result <<
			"\nBuffer was:\n" << buffer << "\n";
	}
	return result;
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
