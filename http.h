/*
 * Copyright (C) 2023 Joseph M Vrba
 *
 * This file contains structures for HTTP requests.
 */
#ifndef HTTP_H
#define HTTP_H

#include <stdint.h>
#include <stdio.h>

#include "pool.h"
#include "str.h"

// The max length of an HTTP parameter.
#define PARAM_NAME_MAX 256
// The max value of an HTTP parameter.
#define PARAM_VALUE_MAX 1024

/*
 * The supported HTTP request types.
 */
enum request_type {
	GET,
	POST
};

/*
 * An HTTP request.
 */
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

/*
 * An extracted HTTP parameter.
 */
struct param {
	char name[PARAM_NAME_MAX];
	char value[PARAM_VALUE_MAX];
};

/*
 * The header for the HTTP 200 OK response.
 */
extern const char ok_header[];

/*
 * Searches for a parameter in the http request.
 *
 * r - The request to search through.
 * param_name - The name of the parameter to look for.
 * value - The str to set to the value of the parameter if found..
 *
 * Returns zero if the parameter was found. The value parameter will be set if
 * zero is returned. Otherwise returns an error code.
 */
int find_param(struct request *r, const struct str *param_name,
	struct str *value);

char *header_find_value(struct request *r, const char *key);

/*
 * Prints a HTTP request for debugging.
 *
 * r - The request to print.
 */
void print_request(struct request *r);

/*
 * Sends a blob of data to the client with the specified header.
 *
 * client - The client to send the data to.
 * header - The HTTP response header to send with the data.
 * contents - The buffer to send to the client.
 * content_len - The length of contents in bytes.
 *
 * Returns 0 if the data was sent to the client. Otherwise returns an error
 * code.
 */
int send_data(int client, const char *header, const char *contents,
	size_t content_len);

/*
 * DEPRECATED Sends a file to the client with the HTTP 200 OK header.
 *
 * f - The file to send to the client.
 * client - The client to send data to.
 * p - The pool to load the file into.
 *
 * Returns 0 if the file was sent to the client successful. Otherwise returns
 * an error code.
 */
int send_file(FILE *f, int client, struct pool *p);

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
