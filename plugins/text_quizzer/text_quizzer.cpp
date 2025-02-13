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

#include <map>
#include <string>
#include <vector>
#include <iostream>

#include "quiz_page.h"
#include "select_quiz_page.h"
#include "startup_page.h"

#define CONFIG_FILE "text_quizzer.conf"

std::map<std::string, std::variant<std::string, int, float>> variables;


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

std::vector<std::string> quiz_files;

enum class state {
	startup,
	quiz_selection,
	in_quiz,
};

state current_state = state::startup;

// Local functions.
static int create_default_config(void);
static int read_entire_file(const char *path, struct file_data *dest);
static std::vector<std::string> split_string(const std::string& str,
	char delimiter);
static void replace_in_page(std::string& page);


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
		page = select_quiz_page;
		break;
	case state::in_quiz:
		page = quiz_page;
		break;
	}

	// Replace the text variables with their values.
	replace_in_page(page);

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
static void replace_in_page(std::string& page)
{
	for (std::string::size_type i = 0; i < page.length(); ++i) {
		if (page.at(i) != '$')
			continue;

		// Found a potential variable, check if it is escaped.
		if (i + 1 >= page.length()) {
			// No more chars, but also no more text so we can't ID
			// the variable anyway. Escape it and return.
			page.insert(i, "$");
			return;
		}
		// Can check if the var is escaped.
		if (page.at(i + 1) == '$') {
			// Its escaped, so continue, but jump past the escaped
			// $, otherwise it'll look like its not escaped. +1
			// should do. The loop will move us +2 total.
			i++;
			continue;
		}

		// Not escaped, so its a variable. Get its name.
		std::string::size_type start = i;

		// Variable ends at the next whitespace or symbol.
		std::string::size_type end;
		for (end = start; end < page.length(); ++end) {
			if (!isalnum(page[end]))
				break;
		}

		// Lookup variable
		auto var_name = page.substr(start, end - start);
		auto entry = variables.find(var_name);
		if (entry == variables.end()) {
			// Just escape the $
			page.insert(start, "$");
			// Jump past the escaped $. Like above, +1 should do.
			// The loop ++ will make it +2.
			i++;
			continue;
		}
		// Otherwise we have the variable so replace the name with its
		// value.
		auto& values = entry->second;
		std::string value{"unrecognized value"};
		if (std::holds_alternative<int>(values)) {
			value = std::to_string(std::get<int>(values));
		} else if (std::holds_alternative<float>(values)) {
			value = std::to_string(std::get<float>(values));
		} else if (std::holds_alternative<std::string>(values)) {
			value = std::get<std::string>(values);
		}

		page.replace(start, end - start, value);
	}
};
