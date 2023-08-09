/*
 * Copyright (C) 2023 Joseph M Vrba
 *
 * This file contains structures for HTTP requests.
 */
#ifndef HTTP_H
#define HTTP_H

enum request_type {
	GET,
	POST
};

struct request {
	enum request_type type;
	char path[FILENAME_MAX];
	char *format;
	size_t header_count;
	struct header {
		char *key;
		char *value;
	} headers[20];
	char buffer[1024];
	size_t param_len;
	size_t param_cap;
	char *parameters;
};

#define PARAM_NAME_MAX 256
#define PARAM_VALUE_MAX 1024

struct param {
	char name[PARAM_NAME_MAX];
	char value[PARAM_VALUE_MAX];
};

const char *ok_header;

/*
 * Sends the 404 error code to the client.
 *
 * 404 is sent when a resource is not found.
 *
 * client - The client to send the message to.
 *
 * Returns 0 if the message was sent. Otherwise an error code is returned.
 */
int send_404(int client);

#endif // HTTP_H
