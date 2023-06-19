#include <assert.h>
#include <winsock2.h>
#include <stdio.h>

const unsigned short port = 8080;

int handle_client(SOCKET client, struct sockaddr_in *client_addr)
{
	return -1;
}

int server(SOCKET server_sock)
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
		address.sin_port = htons(port);

		result = bind(server_sock, (SOCKADDR*)&address, sizeof(address));
		if (0 == result) {
			result = listen(server_sock, SOMAXCONN);
			if (result != SOCKET_ERROR) {
				result = server(server_sock);
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