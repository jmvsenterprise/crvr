/**
 * Copyright (C) 2025 Joseph M Vrba
 */
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http.h"
#include "str.h"
#include "utils.h"

#include "quiz_page.h"
#include "select_quiz_page.h"
#include "startup_page.h"

#define CONFIG_FILE "quizzer.conf"

struct question {
	struct str question;
	struct str answer;
};

struct question *questions = NULL;
long question_count = 0;
long question_cap = 0;

enum var_type {
	VT_STR,
	VT_DSTR,
	VT_ULONG,
	VT_LONG,
	VT_FLOAT,
};
struct variable {
	enum var_type type;
	union data {
		struct str as_str;
		struct dstr as_dstr;
		unsigned long as_ulong;
		long as_long;
		float as_float;
	} data;
};

struct str *var_names = NULL;
struct variable *var_values = NULL;

struct file_data {
	char *data;
	long len;
};

struct file_data config_data = {0};
struct file_data quiz_file = {0};

struct str_array {
	long count;
	long cap;
	struct str *strs;
};

struct str_array quiz_files = {0};
struct str_array lines = {0};

enum state {
	STARTUP,
	QUIZ_SELECTION,
	IN_QUIZ,
};

enum state current_state = STARTUP;

// Local functions.
static void clear_question(struct question *q);
static int create_default_config(void);
static int has_indentation(const struct str *line);
static int load_quiz(const struct str *quiz_name);
static int move_front_q_to(long offset);
/*
 * Lookup and return the variable with the specified name. If the variable
 * doesn't exist, create it and return it.
 */
static struct variable *get_variable(const char *var_name);
static int read_entire_file(const char *path, struct file_data *dest);
/*
 * Remove the first q in the questions array, moving all the questions after
 * it forward.
 */
static int remove_first_q(void);
static int question_is_empty(const struct question *q);
static void replace_in_page(struct dstr *page);
static int split_string(const struct str *str, struct str_array *strs,
	char delimiter);

static void print_question(FILE *f, struct question *q)
{
	fprintf(f, "question: ");
	str_print(f, &q->question);
	fprintf(f, "\nanswer: ");
	str_print(f, &q->answer);
	fputc('\n', f);
}

int load_plugin(void)
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

	struct str config = {.s = config_data.data, .len = config_data.len };

	error = split_string(&config, &quiz_files, '\n');

	printf("Configuration found these quizzes:\n");
	struct variable *quizzes = get_variable("quizzes");
	quizzes->type = VT_DSTR;
	struct dstr *quiz_list_html = &quizzes->data.as_dstr;
	dstr_free(quiz_list_html);
	*quiz_list_html = (struct dstr){0};

	for (long i = 0; i < quiz_files.count; i++) {
		struct str *quiz_file = quiz_files.strs + i;
		putchar('\t');
		str_print(stdout, quiz_file);
		dstr_append_cstr(quiz_list_html, 
			"<li><input type=\"submit\" name=\"button\" id=\"");
		dstr_append_str(quiz_list_html, quiz_file);
		dstr_append_cstr(quiz_list_html, "\" value=\"");
		dstr_append_str(quiz_list_html, quiz_file);
		dstr_append_cstr(quiz_list_html, "\"></li>\n");
	}

	current_state = QUIZ_SELECTION;

	return error;
}

int
unload_plugin(void)
{
	free_variables();
	free_questions();
	free_lines();
	free_quiz();
	return 0;
}

int
handle_get(struct request *r, int client)
{
	(void)r;
	(void)client;

	struct dstr page = {0};
	int error = 0;

	// Load the page text
	switch (current_state) {
	case STARTUP:
		error = dstr_append_cstr(&page, startup_page_html);
		break;
	case QUIZ_SELECTION:
		error = dstr_append_cstr(&page, select_quiz_page_html);
		break;
	case IN_QUIZ:
		error = dstr_append_cstr(&page, quiz_page_html);
		break;
	}

	if (error) {
		fprintf(stderr, "Failed to load page! %i\n", error);
		dstr_free(&page);
		return error;
	}

	// Replace the text variables with their values.
	replace_in_page(&page);

	return send_data(client, ok_header, page.s, (size_t)page.len);
}

static void
handle_poor_value(void)
{
	// If there are no other questions, just show this question again.
	if (question_count == 1) {
		return;
	}

	// Otherwise move the question 1-5 questions out. First check if we
	// have less than 5 questions left:
	long distance;
	if (question_count < 5) {
		// We do, so just move move it among those questions. + 1
		// because we want to move it at least one spot.
		distance = (rand() % question_count) + 1;
	} else {
		// Otherwise move it 1-5 questions out.
		distance = (rand() % 5) + 1;
	}

	move_front_q_to(distance);
}

int
handle_quiz_selection_post(struct request *r)
{
	struct http_param button = {};
	if (find_post_param(r, "button", &button)) {
		fprintf(stderr, "Did not find button\n");
		return EINVAL;
	}
	// Confirm the quiz file is in our list.
	long good_quiz_file = 0;
	for (long i = 0; i < quiz_files.count; ++i) {
		struct str *quiz_file = quiz_files.strs + i;
		if (str_cmp(&button.value, quiz_file) == 0) {
			good_quiz_file = 1;
			break;
		}
	}
	if (good_quiz_file) {
		int error = load_quiz(&button.value);
		if (error) {
			fprintf(stderr, "Failed to load quiz ");
			str_print(stderr, &button.value);
			fprintf(stderr, "%i\n", error);
			return error;
		}
	} else {
		fprintf(stderr, "Unrecognized quiz file: \"");
		str_print(stderr, &button.value);
		fprintf(stderr, "\"\n");
		return 1;
	}
	return 0;
}

int
handle_in_quiz_post(struct request *r)
{
	// See which button the user pressed. We won't do
	// anything with this right now, but get it.
	struct http_param button = {0};
	if (find_post_param(r, "button", &button)) {
		fprintf(stderr, "Did not find button\n");
		return EINVAL;
	}
	printf("User pressed ");
	str_print(stdout, &button.value);
	putchar('\n');

	// Remove the card from the list if they chose 'great'
	if (str_cmp_cstr(&button.value, "great") == 0) {
		remove_first_q();
	} else if (str_cmp_cstr(&button.value, "good") == 0) {
		// Put the card at the end of the deck if they
		// chose "good". But only move it if there is more than one
		// question in the quiz.
		if (question_count > 1) {
			move_front_q_to(question_count - 1);
		}
	} else if (str_cmp_cstr(&button.value, "poor") == 0) {
		handle_poor_value();
	}

	// Is the quiz done?
	if (question_count == 0) {
		// Yep, go back to the quiz selection screen!
		current_state = QUIZ_SELECTION;
	} else {
		// Update variables.
		struct variable *questions_remaining = get_variable(
			"questions_remaining");
		questions_remaining->type = VT_LONG;
		questions_remaining->data.as_long = question_count;

		const struct question *current_q = questions;
		struct variable *q = get_variable("question");
		q->type = VT_STR;
		q->data.as_str = current_q->question;

		struct variable *answer = get_variable("answer");
		answer->type = VT_STR;
		answer->data.as_str = current_q->answer;
	}

	return 0;
}

int
handle_post(struct request *r, int client)
{
	(void)client;

	// Call parse_post_parameters to make them available.
	int error = parse_post_parameters(r);
	if (error) {
		fprintf(stderr, "Error parsing post params: %i\n", error);
		return error;
	}

	// Call find_post_param to find what was selected.
	switch (current_state) {
	case STARTUP:
		// Nothing to do.
		break;
	case QUIZ_SELECTION:
		error = handle_quiz_selection_post(r);
		break;
	case IN_QUIZ:
		error = handle_in_quiz_post(r);
		break;
	}

	// ASL then calls its asl_get to send the page with updated values.
	return handle_get(r, client);
}

static void
clear_question(struct question *q)
{
	q->question = q->answer = (struct str){0};
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
has_indentation(const struct str *line)
{
	assert(line);

	// An empty line has no indentation.
	if (line->len == 0) return 0;

	// If the first char is whitespace, an asterisk, a hypen or a digit,
	// its indented.
	if (isspace(line->s[0]) || line->s[0] == '*' ||
			line->s[0] == '-' || isdigit(line->s[0])) {
		return 1;
	}

	// The line is not indented.
	return 0;
}

static int
question_is_empty(const struct question *q)
{
	return q->question.len == 0 && q->answer.len == 0;
}

static int
load_quiz(const struct str *quiz_name)
{
	char file_name[MAX_FILE_NAME_LEN] = {0};

	puts("loading quiz ");
	str_print(stdout, quiz_name);
	putchar('\n');
	assert(quiz_name.len < MAX_FILE_NAME_LEN);
	memcpy(file_name, quiz_name.s, quiz_name.len);

	FILE *quiz = fopen(file_name, "rb");
	if (quiz) {
		read_in_quiz(quiz);
		fclose(quiz);
	} else {
		fprintf("Failed to open %s: %i", file_name, errno);
		return errno? errno : EEXIST;
	}

	return 0;
}

/*
 * Loop through every line of the file looking for things formatted
 * like this:
 *
 *    # A comment
 *    Question[:?]\n
 *    [ \t]*answer\n
 *
 * Questions are not indented and answers are indented after a question.
 *
 * There can be multiple answer lines under a question and they should
 * all be included as part of the answer.
 */
static int
read_in_quiz(FILE *f)
{
	if (fseek(f, 0, SEEK_END) != 0) {
		fprintf(stderr, "Failed to seek for file length: %i\n", errno);
		return errno? errno : EIO;
	}
	free_quiz();

	quiz_file.len = ftell(f);
	if (quiz_file.len == -1) {
		fprintf(stderr, "Failed to get file length: %i\n", errno);
		return errno? errno : EIO;
	}
	rewind(f);

	quiz_file.data = malloc(quiz_file.len);
	if (!quiz_file.data) {
		fprintf(stderr, "Failed to alloc file buffer: %s\n",
			strerror(errno));
		return errno? errno : ENOMEM;
	}

	size_t bytes_read = fread(quiz_file.data, 1, quiz_file.len, f);
	if (bytes_read != (size_t)quiz_file.len) {
		fprintf(stderr, "Failed to read in quiz file: %s\n",
			strerror(errno));
		return errno? errno : EIO;
	}

	return parse_quiz(&quiz_file);
}

static int
parse_quiz(struct file_data *quiz_data)
{
	long line_start;
	long line_end;

	free_lines();
	for (line_start = 0; line_start < quiz_data->len; ++line_start) {
		// Find the end of the line.
		for (line_end = 0; line_end < quiz_data->len; ++line_end) {
			if (quiz_data->data[line_end] == '\n') {
				break;
			}
		}
		struct str line = {quiz_data->s + line_start, line_end -
			line_start};
		printf("Considering \"");
		str_print(stdout, &line);
		printf("\"...");

		// Skip empty lines.
		if (line.len == 0 || line.len == 1) {
			line_start = line_end;
			printf("empty line\n");
			continue;
		}
		// Skip comment lines.
		int comment = 0;
		for (size_t i = 0; i < line.length(); ++i) {
			// Loop through the line until we find the first non-
			// space character. If that is a '#' its a comment.
			if (isspace(line.s[i]))
				continue;
			if (line.s[i] == '#') {
				comment = 1;
				break;
			}
		}
		if (comment) {
			printf("comment\n");
			line_end = line_start;
			continue;
		}
		
		printf("line\n");
		error = add_quiz_line(&line);
		if (error) {
			printf("Failed to add new line: %i\n", error);
			return error;
		}
	}

	// Now we have a list of all the lines from the file. Go through each
	// line and create a question if the line starts without indentation.
	// Every line after that has indentation is part of the answer.
	struct question q;
	free_questions();

	for (long i = 0; i < lines.count; ++i) {
		if (!has_indentation(&lines.strs[i])) {
			if (!question_is_empty(q)) {
				add_question(questions, q);
				clear_question(q);
			}
			q.question = lines.strs[i];
		} else {
			if (!q.answer.s) {
				q.answer.s = lines.strs[i];
			}
			q.answer.len += lines.strs[i].len;
		}
	}
	// Save the last question being built.
	if (!question_is_empty(q)) {
		add_question(questions, q);
	}

	printf("Loaded these questions:\n");
	for (long i = 0; i < question_count; ++i) {
		str_print(&questions[i].question);
	}
	printf("Original question count: %li\n", question_count);

	// Duplicate and reverse every question.
	const size_t original_end = question_count;
	for (size_t i = 0; i < original_end; ++i) {
		question flop;
		flop.question = questions[i].answer;
		flop.answer = questions[i].question;
		add_question(questions, flop);
	}
	printf("Original + flopped question count: %lu\n", question_count);

	// Shuffle the questions
	shuffle_questions();

	printf("Questions shuffled\n");

	// Set up the variables needed for the quiz page
	set_var_str("quiz_title", quiz_name);
	set_var_ulong("questions_remaining", question_count);

	set_var_str("question", &questions[0].question);
	set_var_str("answer", &questions[0].answer);

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

int
split_string(struct str *str, char delimiter, struct str_array *dst)
{
	assert(str && dst);

	dst->count = 0;
	dst->cap = 0;
	dst->strs = NULL;

	long prev = 0;
	long current = 0;
	for (; current < str->len; ++current) {
		// Skip until we get to a delimiter.
		if (str->s[current] != delimiter) {
			continue;
		}
		// Extract out the str.
		struct str *s = str_array_get_new_str(dst);
		str_get_substr(str, prev, current, s);
		prev = current + 1;
	}
	if (current > prev) {
		struct str *s = str_array_get_new_str(dst);
		str_get_substr(str, prev, current, s);
	}
	return 0;
}

/*
 * Replace the variables found in buf with their values.
 */
static void replace_in_page(struct dstr *page)
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
