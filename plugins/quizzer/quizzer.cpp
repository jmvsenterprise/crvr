/**
 * Copyright (C) 2025 Joseph M Vrba
 */
#include <assert.h>
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

#include <algorithm>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <random>

#include "quiz_page.h"
#include "select_quiz_page.h"
#include "startup_page.h"

#define CONFIG_FILE "text_quizzer.conf"

struct question {
	std::string question;
	std::string answer;
};

std::vector<question> quiz_questions;

std::map<std::string, std::variant<std::string, size_t, int, float>> variables;

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
static void clear_question(question& q);
static int create_default_config(void);
static bool has_indentation(const std::string& line);
static int load_quiz(const std::string& quiz_name);
static int read_entire_file(const char *path, struct file_data *dest);
static bool question_is_empty(const question& q);
static void replace_in_page(std::string& page);
static std::vector<std::string> split_string(const std::string& str,
	char delimiter);

std::ostream& operator<<(std::ostream& os, const question& q)
{
	return os << "question: " << q.question << "\nanswer: " << q.answer <<
		'\n';
}

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
	std::string quiz_var;
	for (const auto& quiz_file : quiz_files) {
		std::cout << '\t' << quiz_file << '\n';
		quiz_var += "<li><input type=\"submit\" name=\"button\" id=\"" +
			quiz_file + "\" value=\"" + quiz_file + "\"></li>\n";
	}
	variables["quizzes"] = quiz_var;

	current_state = state::quiz_selection;

	return error;
}

extern "C" int
unload_plugin(void)
{
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

static void
handle_poor_value(std::vector<question>& questions)
{
	// If there are no other questions, just show this question again.
	if (questions.size() == 1) {
		return;
	}

	// Otherwise move the question 1-5 questions out. First check if we
	// have less than 5 questions left:
	long distance;
	if (questions.size() < 5) {
		// We do, so just move move it among those questions. + 1
		// because we want to move it at least one spot.
		assert(questions.size() < LONG_MAX);
		distance = rand() % (long)questions.size() + 1;
	} else {
		// Otherwise move it 1-5 questions out.
		distance = rand() % 5 + 1;
	}

	question q = questions.at(0);
	questions.insert(questions.begin() + distance, q);
	questions.erase(questions.begin());
}

extern "C" int
handle_post(struct request *r, int client)
{
	(void)client;

	// Call parse_post_parameters to make them available.
	int error = parse_post_parameters(r);
	if (error) {
		std::cout << "Error parsing post params: " << error << '\n';
		return error;
	}

	// Call find_post_param to find what was selected.
	switch (current_state) {
	case state::startup:
		// Nothing to do.
		break;
	case state::quiz_selection:
		{
			http_param button = {};
			if (find_post_param(r, "button", &button)) {
				std::cerr << "Did not find button\n";
				return EINVAL;
			}
			std::string value(button.value.s,
				(size_t)button.value.len);
			// Confirm the quiz file is in our list.
			bool good_quiz_file = false;
			for (const auto& quiz_file : quiz_files) {
				if (value == quiz_file) {
					good_quiz_file = true;
					break;
				}
			}
			if (good_quiz_file) {
				int error = load_quiz(value);
				if (error) {
					std::cerr << "Failed to load quiz " <<
						value << ": " << error << '\n';
				}
			} else {
				std::cout << "Unrecognized quiz file: \"" <<
					value << "\"\n";
			}
		}
		break;
	case state::in_quiz:
		{
			// See which button the user pressed. We won't do
			// anything with this right now, but get it.
			http_param button = {};
			if (find_post_param(r, "button", &button)) {
				std::cerr << "Did not find button\n";
				return EINVAL;
			}
			std::string value(button.value.s,
				(size_t)button.value.len);
			std::cout << "user pressed " << value << '\n';

			// Remove the card from the list if they chose 'great'
			if (value == "great") {
				quiz_questions.erase(quiz_questions.begin());
			} else if (value == "good") {
				// Put the card at the end of the deck if they
				// chose "good".
				if (quiz_questions.size() > 1) {
					question q = quiz_questions.at(0);
					quiz_questions.erase(quiz_questions.begin());
					quiz_questions.emplace_back(q);
				}
			} else if (value == "poor") {
				handle_poor_value(quiz_questions);
			}

			// Is the quiz done?
			if (quiz_questions.size() == 0) {
				// Yep, go back to the quiz selection screen!
				current_state = state::quiz_selection;
			} else {
				// Update variables.
				variables["questions_remaining"] =
					quiz_questions.size();
				const auto& question =
					quiz_questions.at(0);
				variables["question"] = question.question;
				variables["answer"] = question.answer;
			}
		}
		break;
	}

	// ASL then calls its asl_get to send the page with updated values.
	return handle_get(r, client);
}

static void
clear_question(question& q)
{
	q.question.clear();
	q.answer.clear();
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

static bool
has_indentation(const std::string& line)
{
	if (line.empty()) return false;
	// If the first char is whitespace, a asterisk, a hypen or a digit,
	// its indented.
	if (line.length() > 0) {
		if (isspace(line.at(0)) || line.at(0) == '*' ||
			line.at(0) == '-' || isdigit(line.at(0))) {
			return true;
		}
	}
	// Another empty line case I guess.
	return false;
}

static bool
question_is_empty(const question& q)
{
	return q.question.empty() && q.answer.empty();
}

static int
load_quiz(const std::string& quiz_name)
{
	std::cout << "loading quiz " << quiz_name << '\n';
	/*
	 * Loop through every line of the file looking for things formatted
	 * like this:
	 *
	 *    # A comment
	 *    Question[:?]\n
	 *    [ \t]*answer\n
	 *
	 * Questions end with either an optional colon (':'), an optional
	 * question mark ('?') and a mandatory newline ('\n').
	 *
	 * Answers are indented after a question.
         *
	 * There can be multiple answer lines under a question and they should
	 * all be included as part of the answer.
	 */
	std::fstream quiz{quiz_name, std::ios::in | std::ios::binary};
	std::vector<std::string> lines;
	for (std::string line; std::getline(quiz, line); ) {
		std::cout << "Considering \"" << line << "\"...";
		// Skip empty lines.
		if (line.empty()) {
			std::cout << "empty line\n";
			continue;
		}
		// Skip comment lines.
		bool comment = false;
		for (size_t i = 0; i < line.length(); ++i) {
			// Loop through the line until we find the first non-
			// space character. If that is a '#' its a comment.
			if (isspace(line.at(i)))
				continue;
			if (line.at(i) == '#') {
				comment = true;
				break;
			}
		}
		if (comment) {
			std::cout << "comment\n";
			continue;
		}
		std::cout << "line\n";
		lines.emplace_back(line);
	}

	// Now we have a list of all the lines from the file. Go through each
	// line and create a question if the line starts without indentation.
	// Every line after that has indentation is part of the answer.
	question q;
	quiz_questions.clear();
	for (size_t i = 0; i < lines.size(); ++i) {
		if (!has_indentation(lines.at(i))) {
			if (!question_is_empty(q)) {
				quiz_questions.emplace_back(q);
				clear_question(q);
			}
			q.question = lines.at(i);
		} else {
			if (!q.answer.empty())
				q.answer += '\n';
			q.answer += lines.at(i);
		}
	}
	// Save the last question being built.
	if (!question_is_empty(q)) {
		quiz_questions.emplace_back(q);
	}

	std::cout << "Loaded these questions:\n";
	for (const auto& q : quiz_questions) {
		std::cout << q << '\n';
	}
	std::cout << "Original question count: " << quiz_questions.size() <<
		'\n';

	// Duplicate and reverse every question.
	const size_t original_end = quiz_questions.size();
	for (size_t i = 0; i < original_end; ++i) {
		question flop;
		flop.question = quiz_questions.at(i).answer;
		flop.answer = quiz_questions.at(i).question;
		quiz_questions.emplace_back(flop);
	}
	std::cout << "Original + flopped question count: " <<
		quiz_questions.size() << '\n';

	// Shuffle the questions
	std::random_device device;
	std::mt19937 randomizer(device());
	std::shuffle(quiz_questions.begin(), quiz_questions.end(), randomizer);

	std::cout << "Questions shuffled.\n";

	// Set up the variables needed for the quiz page
	variables["quiz_title"] = quiz_name;
	variables["questions_remaining"] = quiz_questions.size();

	variables["question"] = quiz_questions.at(0).question;
	variables["answer"] = quiz_questions.at(0).answer;

	current_state = state::in_quiz;

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

		// Not escaped, so its a variable. Get its name. Move past the
		// $ first though so +1.
		std::string::size_type start = i + 1;

		// Variable ends at the next whitespace or symbol. But check for
		// an underscore too, which continues the name.
		std::string::size_type end;
		for (end = start; end < page.length(); ++end) {
			if (!isalnum(page[end]) && (page[end] != '_'))
				break;
		}

		// Lookup variable
		auto var_name = page.substr(start, end - start);
		std::cout << "Looking for variable \"" << var_name << "\"...";
		auto entry = variables.find(var_name);
		if (entry == variables.end()) {
			std::cout << "not found\n";
			// Just escape the $
			page.insert(start, "$");
			// Jump past the escaped $. Like above, +1 should do.
			// The loop ++ will make it +2.
			i++;
			continue;
		}
		std::cout << "found!\n";
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
		} else if (std::holds_alternative<size_t>(values)) {
			value = std::to_string(std::get<size_t>(values));
		}

		// +1 because we are starting at the '$' which is at i.
		page.replace(i, end - start + 1, value);
	}
};
