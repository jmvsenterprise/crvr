/*
 * Copyright (C) 2023 Joseph M Vrba
 *
 * Contains definitions and routines for HTTP requests.
 */
#include "http.h"

const char ok_header[] = "HTTP/1.1 200 OK";

int find_param(struct param out[static 1], struct request r[static 1],
	const char *param_name)
{
	*out = (struct param){0};

	if (!param_name)
		return EINVAL;
	char *param = strstr(r->parameters, param_name);
	if (!param) {
		printf("Did not find %s in parameters.\n", param_name);
		return EINVAL;
	}

	printf("param: \"%s\"\n", param);

	size_t param_name_len = strlen(param_name);
	(void)strncpy(out->name, param_name, param_name_len);

	param += param_name_len;
	printf("after name param: \"%s\"\n", param);
	if ('=' != *param) {
		printf("Param is not '=': \"%c\" (0x%hhx)\n", *param, *param);
		return EINVAL;
	}
	param += sizeof((char)'=');
	printf("after = param: \"%s\"\n", param);

	/*
	 * Look for a \r\n or \0. If either one is encountered that's the end
	 * of the parameter value.
	 */
	size_t value_end = 0;
	printf("Checking param: ");
	for (; param[value_end]; ++value_end) {
		printf("'%c', ", param[value_end]);
		if (('\n' == param[value_end]) && (value_end > 0) &&
				(param[value_end - 1] == '\r')) {
			value_end -= (sizeof((char)'\r') + sizeof((char)'\n'));
			break;
		}
	}
	printf("\n");

	for (size_t i = 0; i < value_end; ++i) {
		out->value[i] = param[i];
	}
	printf("param %s:%s.\n", out->name, out->value);

	return 0;
}

int send_404(int client)
{
	static const char html[] = 
		"<html>"
		"  <head>"
		"    <title>Page Not Found</title>"
		"  </head>"
		"  <body>"
		"    <h1>Sorry that page doesn't exist</h1>"
		"  </body>"
		"</html>";
	static const char header[] = "HTTP/1.1 404 NOT FOUND";

	return send_data(client, header, html, STRMAX(html));
}

