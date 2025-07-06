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
#define MAX_FILE_NAME_LEN 512

struct question {
	struct str question;
	struct str answer;
};

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

struct file_data {
	char *data;
	long len;
};

struct str_array {
	long count;
	long cap;
	struct str *strs;
};

struct question *questions = NULL;
long question_count = 0;
long question_cap = 0;

struct str *var_names = NULL;
struct variable *var_values = NULL;
long var_count = 0;
long var_cap = 0;

struct file_data config_data = {0};
struct file_data quiz_file = {0};

struct str_array quiz_files = {0};
struct str_array lines = {0};

enum state {
	STARTUP,
	QUIZ_SELECTION,
	IN_QUIZ,
};

enum state current_state = STARTUP;

// Local functions.
static int add_question(struct question *questions, struct question *new_q);
static void clear_question(struct question *q);
static int create_default_config(void);
static void free_lines(void);
static void free_questions(void);
static void free_quiz(void);
static void free_variables(void);
static int has_indentation(const struct str *line);
static int load_quiz(const struct str *quiz_name);
static int move_front_q_to(long offset);
static int parse_quiz(struct file_data *quiz_data);
static int read_in_quiz(FILE *f);
static int str_array_add(struct str_array *arr, const struct str *str);
static void str_array_free(struct str_array *arr);
/*
 * Lookup and return the variable with the specified name. If the variable
 * doesn't exist, create it and return it.
 */
static struct variable *get_var(const struct str *name);
static struct variable *get_var_cstr(char *var_name);
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
/* Set variable values */
static int set_var_str(const char* name, const char* value);
static int set_var_ulong(const char* name, unsigned long value);

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
	dstr_free(&quizzes->data.as_dstr);

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

static int
add_question(struct question *questions, struct question *new_q)
{
	struct question *new_array;
	long new_cap;

	if (!questions || !new_q) return EINVAL;
	if (question_count >= question_cap) {
		if (question_cap > 0) {
			if (question_cap == LONG_MAX) {
				return ENOMEM;
			}
			new_cap = question_cap * 2;
			// Overflow check
			if (new_cap < question_cap) {
				new_cap = LONG_MAX;
			}
		} else {
			question_cap = 10;
		}
		new_array = calloc(new_cap, sizeof(*new_array));
		if (!new_array) {
			return ENOMEM;
		}
		for (long i = 0; i < question_count; ++i) {
			new_array[i] = questions[i];
		}
		if (questions)
			free(questions);
		questions = new_array;
		question_cap = new_cap;
	}
	questions[question_count] = *new_q;
	question_count++;
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

static void
free_lines(void)
{
	str_array_free(lines);
}

static void
free_quiz(void)
{
	free(quiz_file.data);
	quiz_file.data = NULL;
	quiz_file.len = 0;
}

static void
free_questions(void)
{
	free(questions);
	questions = NULL;
	question_count = question_cap = 0;
}

static void
free_variables(void)
{
	free(var_names);
	free(var_values);
	var_names = NULL;
	var_values = NULL;
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
remove_first_q(void)
{
	if (question_count <= 0) {
		return 0;
	}
	if (question_count == 1) {
		question_count = 0;
		return 0;
	}
	question_count--;
	memmove(&question[0], &question[1],
		sizeof(*questions) * (unsigned long)question_count);
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
	assert(quiz_name->len < MAX_FILE_NAME_LEN);
	memcpy(file_name, quiz_name->s, quiz_name->len);

	FILE *quiz = fopen(file_name, "rb");
	if (quiz) {
		read_in_quiz(quiz);
		fclose(quiz);
	} else {
		fprintf(stderr, "Failed to open %s: %i", file_name, errno);
		return errno? errno : EEXIST;
	}

	return 0;
}

static int
move_front_q_to(long offset)
{
	struct question tmp;
	if (offset < 0 || offset >= question_count) {
		return EINVAL;
	}
	// 0 is the offset of the front question, so skip moving in that case.
	if (offset == 0) return 0;
	tmp = questions[0];
	// Shift all questions forward one position up to the offset we want
	// to fill in. Questions following offset don't need to move.
	memmove(&questions[0], &questions[1], sizeof(*questions) *
		(unsigned long)offset);
	questions[offset] = tmp;
	return 0;
}

static int
str_array_add(struct str_array *arr, const struct str *str)
{
	struct str *new_arr;
	long new_cap;
	if (!arr || !str) return EINVAL;
	/* If the array hasn't been initialized, set it up so it goes into the
	 * code below that allocates an array.*/
	if (!arr->strs) {
		arr->count = 0;
		arr->cap = 0;
	}
	if (arr->count >= arr->cap) {
		/* Can't expand any further than this. */
		if (arr->cap == LONG_MAX) {
			return ENOBUFS;
		}
		if (arr->count != 0) {
			/* Double the current count. */
			new_cap = arr->count * 2;
		} else {
			/* New array, start with a default amount */
			new_cap = 10;
		}
		/* Clamp if capacity overflowed. */
		if (new_cap < 0) {
			new_cap = LONG_MAX;
		}
		struct str *new_arr = calloc((unsigned long)new_cap,
			sizeof(*new_arr));
		if (!new_arr) {
			return errno? errno : ENOMEM;
		}
		memcpy(new_arr, arr->strs, arr->count);
		if (arr->strs)
			free(arr->strs);
		arr->strs = new_arr;
		arr->cap = new_cap;
	}
	// Can add the item at count.
	arr->strs[arr->count] = *str;
	arr->count++;
	return 0;
}

static void
str_array_free(struct str_array *arr)
{
	if (arr && arr->strs) {
		free(arr->strs);
	}
	arr->strs = NULL;
	arr->count = arr->cap = 0;
}

static struct variable *
get_var(const struct str *name)
{
	long i;
	for (i = 0; i < var_count; ++i) {
		if (0 == str_cmp(&var_names[i], name)) {
			return &var_values[i];
		}
	}
	return create_var(name);
}

static struct variable *
get_var_cstr(char *var_name)
{
	struct str s;
	s.s = var_name;
	unsigned long len = strlen(var_name);
	if (len > (unsigned long)LONG_MAX) {
		fprintf(stderr, "var name: %s is too long. Trimming.\n",
			var_name);
		len = LONG_MAX;
	}
	s.len = (long)len;
	return get_var(&s);
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

	unsigned long quiz_len = (unsigned long)quiz_file.len;
	quiz_file.data = malloc(quiz_len);
	if (!quiz_file.data) {
		fprintf(stderr, "Failed to alloc file buffer: %s\n",
			strerror(errno));
		return errno? errno : ENOMEM;
	}

	size_t bytes_read = fread(quiz_file.data, 1, quiz_len, f);
	if (bytes_read != (size_t)quiz_len) {
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
	int error = 0;

	free_lines();
	for (line_start = 0; line_start < quiz_data->len; ++line_start) {
		// Find the end of the line.
		for (line_end = 0; line_end < quiz_data->len; ++line_end) {
			if (quiz_data->data[line_end] == '\n') {
				break;
			}
		}
		struct str line;
		line.s = quiz_data->data + line_start;
		line.len = line_end - line_start;
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
		for (size_t i = 0; i < line.len; ++i) {
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
		error = str_array_add(&lines, &line);
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
			if (!question_is_empty(&q)) {
				add_question(questions, &q);
				clear_question(&q);
			}
			q.question = lines.strs[i];
		} else {
			if (!q.answer.s) {
				q.answer.s = lines.strs[i].s;
			}
			q.answer.len += lines.strs[i].len;
		}
	}
	// Save the last question being built.
	if (!question_is_empty(&q)) {
		add_question(questions, &q);
	}

	printf("Loaded these questions:\n");
	for (long i = 0; i < question_count; ++i) {
		str_print(stdout, &questions[i].question);
	}
	printf("Original question count: %li\n", question_count);

	// Duplicate and reverse every question.
	const size_t original_end = (size_t)question_count;
	for (size_t i = 0; i < original_end; ++i) {
		struct question flop;
		flop.question = questions[i].answer;
		flop.answer = questions[i].question;
		add_question(questions, &flop);
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

static int
split_string(const struct str *str, struct str_array *dst, char delimiter)
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
	long i;
	for (i = 0; i < page->len; ++i) {
		// Find a variable.
		if (page->s[i] != '$')
			continue;
		// Found a potential variable, check if it is escaped.
		if (i + 1 >= page->len) {
			// No more chars & no more text. Can't ID the variable
			// anyway. Escape it & return.
			dstr_insert_cstr(page, i, "$");
			return;
		}
		// Can check if the var is escaped.
		if (page->s[i + 1] == '$') {
			// It's escaped, continue. Jump past escaped $. The
			// loop will move us +2 total.
			i++;
			continue;
		}
		// Its a variable. Get name. Move past the $ first.
		long start = i + 1;
		// Variable ends at next whitespace or symbol. Check for
		// underscore, which continues name.
		long end;
		for (end = start; end < page->len; ++end) {
			if (!isalnum(page->s[end]) && (page->s[end] != '_'))
				break;
		}

		// Lookup variable
		struct str tmp = {page->s, page->len};
		struct str var_name;
		int error = str_get_substr(&tmp, start, end, &var_name);
		struct variable *entry = NULL;
		if (error) {
			printf("Failed to get var name %i", error);
		} else {
			printf("Looking for variable \"");
			str_print(stdout, &var_name);
			printf("\"...");
			entry = get_var(&var_name);
		}
		if (entry == NULL) {
			printf("not found\n");
			// Just escape the $
			dstr_insert_cstr(page, start, "$");
			// Jump past the escaped $. Loop also ++s
			i++;
			continue;
		}
		printf("found!\n");
		// Otherwise we have the variable so replace the name with its
		// value.
		char var_as_str[512] = {0};
		// +1 because we are starting at the '$' which is at i.
		long var_len = end - start + 1;
		switch (entry->type) {
		case VT_LONG:
			snprintf(var_as_str, sizeof(var_as_str) - 1, "%li",
				entry->data.as_long);
			error = dstr_replace_with_cstr(page, i, var_len,
				var_as_str);
			assert(error == 0); // TODO: handle
			break;
		case VT_ULONG:
			snprintf(var_as_str, sizeof(var_as_str) - 1, "%lu",
				entry->data.as_ulong);
			error = dstr_replace_with_cstr(page, i, var_len,
				var_as_str);
			assert(error == 0); // TODO: handle
			break;
		case VT_FLOAT:
			snprintf(var_as_str, sizeof(var_as_str) - 1, "%f",
				entry->data.as_float);
			error = dstr_replace_with_cstr(page, i, var_len,
				var_as_str);
			assert(error == 0); // TODO: handle
			break;
		case VT_STR:
			error = dstr_replace_with(page, i, var_len,
				entry->data.as_str);
			assert(error == 0); // TODO: handle
			break;
		case VT_DSTR:
			struct str tmp = {.s = entry->data.as_dstr.s,
				.len = entry->data.as_dstr.len};
			error = dstr_replace_with(page, i, var_len,
				&tmp);
			assert(error == 0); // TODO: Handle
			break;
		default:
			fprintf(stderr, "Unrecognized var type: %lu",
				(unsigned long)entry->type);
			break;
		}
	}
};

static struct variable*
create_var(struct str *name)
{
#error todo
}

static int
set_var_str(const struct str* name, const struct str* value)
{
	struct variable *var = get_var(name);
	if (!var) return ENOBUFS;
	var->type = VT_STR;
	var->data.str = str;
	return 0;
}

static int
set_var_ulong(const struct str* name, unsigned long value)
{
	struct variable *var = get_var(name);
	if (!var) return ENOBUFS;
	var->type = VT_ULONG;
	var->data.ulong = value;
	return 0;
}
