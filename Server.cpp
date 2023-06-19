#include <assert.h>
#include <winsock2.h>
#include <stdio.h>
#include <string>
#include <iostream>

namespace server {

#define LEN(p) (sizeof(p) / sizeof(p[0]))

const unsigned short port = 8080;

enum request_type {
	GET,
	POST
};

struct request {
	request_type type;
	std::string path;

	int parse(std::string &data);
};

int request::parse(std::string &data)
{
	size_t first_newline = data.find_first_of('\n');
	if (data.npos == first_newline) {
		std::cerr << "Did not find header EOL.\n";
		return -1;
	}

	std::string header{data.substr(0, first_newline)};

	size_t end_of_type = header.find_first_of(' ');
	if (header.npos == end_of_type) {
		std::cerr << "Did not find request type.\n";
		return -1;
	}

	size_t end_of_path = header.find_first_of(' ', end_of_type + 1);
	if (header.npos == end_of_path) {
		std::cerr << "Did not find path.\n";
		return -1;
	}

	std::string type{header.substr(0, end_of_type)};
	path = header.substr(end_of_type + 1, end_of_path - end_of_type - 1);

	std::cout << "Type: \"" << type << "\" path: \"" << path << "\"\n";

	return -1;
}

int handle_client(SOCKET client, struct sockaddr_in *client_addr)
{
	char buffer[4096] = {0};
	int bytes_rxed = recv(client, buffer, LEN(buffer) - 1, 0);
	if (bytes_rxed == SOCKET_ERROR) {
		printf("Failed to read from client: %d.\n", WSAGetLastError());
		return -1;
	}

	struct request request;
	request.parse(std::string(buffer));

	// Parse the data. But for now just print it out:
	//printf("%s", buffer);

	return -1;
}

int serve(SOCKET server_sock)
{
	int result = 0;
	int keep_running = 1;
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
			result = handle_client(client, &client_addr);
			closesocket(client);

			if (result != 0) {
				printf("Handling the client failed: %d\n", result);
				keep_running = 0;
			}
		} else {
			printf("Error accepting client connection: %d.\n", WSAGetLastError());
			return -1;
		}
	}
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