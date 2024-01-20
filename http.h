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
// The maximum number of header parameters that can be processed in a request.
#define MAX_HEADER_LINES 32
// The maximum number of post parameters that can be processed in a reqest.
#define MAX_POST_PARAMS 32

/**
 * @brief The supported HTTP request types.
 */
enum request_type {
	GET,
	POST
};

/**
 * @brief An HTTP parameter.
 */
struct http_param {
	struct str key;
	struct str value;
};

/**
 * @brief An HTTP request that is sent to and from a client.
 */
struct request {
	enum request_type type;
	struct str path;
	struct str format;
	long header_count;
	struct http_param headers[MAX_HEADER_LINES];
	struct str buffer;
	struct str post_params_buffer;
	long post_param_count;
	struct http_param post_params[MAX_POST_PARAMS]; 
};

/*
 * The header for the HTTP 200 OK response.
 */
extern const char ok_header[];

/**
 * @brief Searches for a parameter in the http request.
 *
 * @param[out] out - The location to save the parameter data when found.
 * @param[in] r - The request to search through.
 * @param[in] param_name - The name of the parameter to look for.
 *
 * @return Returns zero if the parameter was found or ENOENT if it wasn't
 *         found. Returns an error code on error.
 */
int find_param(const struct request *r, const char *param_name,
	struct http_param *out);

/**
 * @brief Searches the POST params in the request.
 *
 * @param[in] r - The request to search.
 * @param[in] param_name - The name of the parameter to search for.
 * @param[out] out - A location to store the parameter key and value.
 *
 * @return Returns 0 if the parameter was found or ENOENT if it wasn't found.
 *         Returns an error code on error.
 */
int find_post_param(const struct request *r, const char *param_name,
	struct http_param *out);

/**
 * @brief Lookup a header parameter in the request.
 *
 * @param[in] r - The request to search.
 * @param[in] key - The key to look up.
 * @param[out] value - The location to store the value.
 *
 * @return Returns 0 if the value was found and stored in the output parameter.
 *         Returns ENOENT if the value wasn't found. Otherwise returns an error
 *         code.
 */
int header_find_value(struct request *r, const char *key, struct str *value);

/**
 * @brief Parse the buffer into an http request.
 *
 * @param[in] data - The buffer to parse.
 * @param[in] data_len - The length of the buffer at data.
 * @param[out] request - The location to store the request data at.
 * @param[in] p - A pool to use for allocations.
 *
 * @return Returns 0 if the buffer was successfully parsed into a request.
 *         Otherwise an error code is returned.
 */
int parse_request(char *data, long data_len, struct request *request,
	struct pool *p);

/**
 * @brief Set up the post_params array from the post_params_buffer.
 *
 * The crvr routines hand a POST request to submodules without parsing the post
 * parameters (key=value) because it doesn't want to make assumptions about
 * what the modules might have in their requests. Instead it will populate the
 * request's post_params_buffer.
 *
 * This function does a generic parse of the request's post_params_buffer
 * looking for key=value pairs and storing each one in the request's post_params
 * array. Then a module can call find_post_param to look up each param as
 * needed.
 *
 * @param[in] r - The request to parse the POST params for.
 *
 * @return Returns 0 if no error occurs. Otherwise returns an error code.
 */
int parse_post_parameters(struct request *r);

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

/**
 * @file Sends the file with the specified path to a client.
 *
 * @param[in] file_path - The path to the file to send.
 * @param[in] client - The client to send the file to.
 * @param[in] p - The pool to use to allocate space to send the file to the
 *                client.
 *
 * @return Returns 0 if the file is sent successfully. Otherwise returns an
 *         error code.
 */
int send_path(struct str *file_path, int client, struct pool *p);

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
