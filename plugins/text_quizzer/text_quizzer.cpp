/**
 * Copyright (C) 2025 Joseph M Vrba
 */
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "http.h"
#include "str.h"
#include "utils.h"
}

#include <string>
#include <vector>
#include <iostream>

#include "quiz_page.h"
#include "select_quiz_page.h"
#include "startup_page.h"

#define CONFIG_FILE "text_quizzer.conf"

struct quiz_question {
	std::string id;
	struct str question;
	struct str answer;
	unsigned long great_recalls;
	unsigned long good_recalls;
	unsigned long poort_recalls;
};

struct file_data {
	char *data;
	long len;
};

struct file_data config_data = {};

static int create_default_config(void);
static int read_entire_file(const char *path, struct file_data *dest);
static std::vector<std::string> split_string(const std::string& str,
	char delimiter);

std::vector<std::string> quiz_files;

enum class state {
	startup,
	quiz_selection,
	in_quiz,
};

state current_state = state::startup;

extern "C" int load_plugin(void)
{
	int error = 0;

	error = read_entire_file(CONFIG_FILE, &config_data);
	// Need to build the file if it doesn't exist, but wait to see what
	// message we get.
	if (error == ENOENT) {
		printf("%s: Config file not found, creating default\n",
			__FILE__);
		error = create_default_config();
		if (error) {
			printf("%s: Failed to create default config: %i\n",
				__FILE__, error);
			return error;
		}
		error = read_entire_file(CONFIG_FILE, &config_data);
	}

	if (error) {
		printf("%s: Failed to read default config: %i\n",
			__FILE__, error);
		return error;
	}

	printf("%s: Default config:\n\"%s\"\n", __FILE__, config_data.data);

	std::string config{config_data.data};

	quiz_files = split_string(config, '\n');

	std::cout << "Configuration found these quizzes:\n";
	for (const auto& quiz_file: quiz_files) {
		std::cout << '\t' << quiz_file << '\n';
	}

	current_state = state::quiz_selection;

	return error;
}

extern "C" int
unload_plugin(void)
{
	return 0;
}

extern "C" int
handle_post(struct request *r, int client)
{
	(void)r;
	(void)client;

	// Call parse_post_parameters to make them available.

	// Call find_post_param to find what was selected.

	// ASL then calls its asl_get to send the page with updated values.
	return 0;
}

extern "C" int
handle_get(struct request *r, int client)
{
	(void)r;
	(void)client;

	std::string page;

	// Load the page text
	switch (current_state) {
	case state::startup:
		page = startup_page;
		break;
	case state::quiz_selection:
		page = select_page_quiz;
		break;
	case state::in_quiz:
		page = quiz_page;
		break;
	}

	// Replace the text variables with their values.
	repace_in_page(page, variables);

	return send_data(client, ok_header, page.c_str(), page.length());
}

static int
create_default_config(void)
{
	FILE *f = fopen(CONFIG_FILE, "wb");
	if (!f) {
		printf("Failed to create %s: %i-%s\n", CONFIG_FILE, errno,
			strerror(errno));
		return errno? errno: EPERM;
	}

	if (fprintf(f, "%s", "example_quiz.txt") < 0) {
		printf("Failed to write default value to %s: %i-%s\n",
			CONFIG_FILE, errno, strerror(errno));
		return errno? errno: EIO;
	}

	fclose(f);

	return 0;
}

static int
read_entire_file(const char *path, struct file_data *dest)
{
	FILE *f = NULL;
	long length = 0;
	size_t items_read = 0;
	int error = 0;

	if (!dest || dest->data) {
		return EINVAL;
	}

	f = fopen(path, "rb");
	if (!f) {
		printf("Failed to find file: %s: %s\n", path, strerror(errno));
		error = errno ? errno : EINVAL;
		goto cleanup;
	}

	fseek(f, 0, SEEK_END);
	length = ftell(f);

	printf("file is length %li\n", length);

	rewind(f);

	dest->len = length;
	// +1 for null in case someone wants to print the contents.
	dest->data = (char*)malloc((unsigned long)length + 1);
	if (!dest->data) {
		printf("Failed to allocate block for file %s: %s\n", path,
			strerror(errno));
		error = errno? errno: ENOMEM;
		goto cleanup;
	}
	memset(dest->data, 0, (unsigned long)(length + 1));

	items_read = fread(dest->data, 1, (unsigned long)dest->len, f);
	if ((long)items_read != dest->len) {
		printf("Failed to read in %s. Read %lu not %li: %s.\n", path,
			items_read, dest->len, strerror(errno));
		error = errno? errno: -1;
	}

	printf("Read %li of %lu items from %s.\n", items_read, dest->len,
		path);

	goto good_cleanup;

cleanup:
	if (dest->data) {
		free(dest->data);
		dest->data = 0;
		dest->len = 0;
	}
good_cleanup:
	if (f) {
		fclose(f);
	}
	return error;
}

static std::vector<std::string>
split_string(const std::string& str, char delimiter)
{
	std::vector<std::string> collection;
	std::string::size_type prev = 0;
	for (;;) {
		auto position = str.find_first_of(delimiter, prev);
		if (position == str.npos) {
			collection.emplace_back(str.substr(prev));
			return collection;
		}

		collection.emplace_back(str.substr(prev, position - prev));
		prev = position + 1;
	}
	return collection;
}

/*
 * Replace the variables found in buf with their values.
 */
static int replace_in_page(std::string& page,
	std::map<std::string, std::string>& variables)
{
	for (;;) {
		auto variable_start = find_first_of(page, '$');
		if (variable_start == page.npos) {
			return;
		}
		// Variable ends at the next whitespace or symbol.
		for (std::string::size_type end = variable_start;
				end < page.length(); ++end) {
			if (!isalnum(page[end]))
				break;
		}
		if (end == page.length()) {
			end = page.npos;
		}

		// Lookup variable
		std::string var_name = page.substr(variable_start,
			end - variable_start);
		std::string var_value = variables[var_name];

		// Replace the variable name with the variable value.
		replace_variable_with_value(page, variable_start, end,
			var_value);
	}
};

struct variable {
	char name[256];
	char *value;

	struct variable *next;
	struct variable *prev;
};

static replace_in_buf(char *buf, size_t *buf_len, const size_t buf_cap,
	struct variable *variables)
{
	size_t dst = 0;
	int result = 0;
	char *var_start;

	for (; (dst < *buf_len) && (*buf_len < buf_cap) && (result == 0);
			++dst) {
		// Look for the start of a variable
		if (buf[dst] != '$') {
			continue;
		}

		// Found a variable
		var_start = buf + dst;
		printf("variable dst:%lu buf_len:%lu \"%.20s\"\n", dst,
			*buf_len, var_start);
		// Find end of variable.
		for (var_end = var_start + 1; var_end < *buf_len; var_end++) {
			if (!isalnum(buf[var_end]))
				break;
		}

		struct variable var;
		struct str var_name;
		var_name.s = var_start;
		var_name.len = var_end - var_start;

		if (0 != find_variable(&var_name, &var)) {
			// Not a variable.
			continue;
		}

		// Found variable, replace its name with the value.
		result = replace_var_with_value(var_start, buf_len, buf_cap,
			var);
	}
	return result;
}

/*
 * Print the value of a variable to the buffer.
 *
 * buf - The buffer to replace the variable in.
 * buf_len - The current length of valid data in the buffer.
 * buf_cap - The max size the buffer can be. buf_len should be < buf_cap.
 * var_name - The name of the variable being replaced.
 * format - The format of the string to replace the variable with.
 * ... - Additional arguments that go into the format.
 *
 * Returns 0 if the variable was successfully replaced in the buffer.
 */
int replace_var_with_value(char *buf, size_t *buf_len, const size_t buf_cap,
	const struct variable *var)
{
	int result;
	char var_str[KILOBYTE] = {0};
	size_t var_len = strlen(var->value);
	size_t buf_space;
	size_t var_name_len = strlen(var->name);

	/* Compute how much space is needed. */
	buf_space = buf_cap - *buf_len;
	if (var_len > buf_space) {
		fprintf(stderr, "Need %lu more bytes in buffer.\n",
			var_len - buf_space);
		return ENOBUFS;
	}

	/* Move the rest of the buffer down and insert the variable. */
	printf("var_len: %lu var_name_len: %lu *buf_len: %lu.\n", var_len,
		var_name_len, *buf_len);
	memmove(buf, buf - var_len + var_name_len, *buf_len + var_len);
	*buf_len += var_len - var_name_len;

	/* Write the variable in. */
	memcpy(buf, var->value, strlen(var->value));

	return result;
}
