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

// Local functions

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

	str_alloc(p, bytes_needed, &r->post_params_buffer);
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

	printf("No post response\n");

	send_path(&r->path, client, p);

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
	int err = parse_request(buffer, LEN(buffer), &request, p);
	if (err) {
		fprintf(stderr,
			"Failed to parse client's request (%i).\n"
			"Buffer was:\n%s\n", err, buffer);
		return -1;
	}
	err = 0;
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

