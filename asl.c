/*
 * Copyright (C) 2023 Joseph M Vrba
 *
 * This file contains functions for the American Sign Language web application
 * for crvr.
 */
#include "asl.h"

#include <errno.h>
#include <dirent.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "utils.h"

// Indicates if a user's confidence level has been tested or not
#define NOT_TESTED -1

static size_t card_count = 0;
static struct card cards[100];
static size_t quiz_len = 0;
static struct quiz_item quiz[LEN(cards) * 2];
static size_t current_quiz_item = 0;
const char asl_file[] = "asl.html";

int find_image_files(void);
int is_image(const char *file);
int show_done_page(int client);
/*
 * Load the file in and replace variables in it. Then send the file to the
 * client. This routine only supports reading in a 4k file. And it allocates
 * enough room to add 2k of parameter data to that file.
 *
 * It returns 0 on success or an errno on failure.
 */
int send_file_with_replaced_params(FILE *f, int client);
void shuffle_cards(void);

int asl_init(void)
{
	if (find_image_files() != 0) {
		fprintf(stderr, "Failed to find image files.\n");
		return -1;
	}
	shuffle_cards();
	return 0;
}

int asl_get(struct request *r, int client)
{
	(void)r;

	static char file_buf[MEGABYTE];
	size_t file_len;
	if (load_file(asl_file, file_buf, LEN(file_buf), &file_len)) {
		fprintf(stderr, "Failed to load %s.\n", asl_file);
		return send_404(client);
	}
	if (replace_in_buf(file_buf, &file_len, LEN(file_buf))) {
		fprintf(stderr, "Failed to replace variables in file.\n");
		return send_404(client);
	}
	return send_data(client, ok_header, file_buf, file_len);
}

int asl_post(struct request *r, int client)
{
	(void)client;

	const char poor_btn[] = "poor";
	const char good_btn[] = "good";
	const char great_btn[] = "great";

	printf("params=\"%s\"\n", r->parameters);

	struct param button = {0};
	if (find_param(&button, r, "button") != 0) {
		// No button param!
		perror("No button param in parameters.\n");
		return EINVAL;
	}

	printf("button param: %s:%s.\n", button.name, button.value);

	struct quiz_item *card = quiz + current_quiz_item;
	if (strcmp(poor_btn, button.value) == 0) {
		// Review this card again during this quiz and reduce the
		// confidence by half.
		card->confidence = (int)((float)card->confidence * 0.5);
	} else if (strcmp(good_btn, button.value) == 0) {
		// Boost the confidence by 1 and review this card that many
		// days in the future.
		card->confidence += 1;
		card->next_review = time(NULL) + (SECONDS_PER_DAY *
			card->confidence);
	} else if (strcmp(great_btn, button.value) == 0) {
		// Double the confidence and review the card that many days in
		// the future.
		card->confidence *= 2;
		card->next_review = time(NULL) + (SECONDS_PER_DAY *
			card->confidence);
	} else {
		printf("Unrecognized button value: \"%s\"\n", button.value);
	}
	current_quiz_item++;
	if (current_quiz_item > quiz_len) {
		// Show done page and show score!
		show_done_page(client);
	} else {
		return asl_get(r, client);
	}

	return 0;
}


/*
 * Create a new card entry, then create two quiz items in the quiz for the card.
 * One for the front of the card and one for the back of the card.
 */
int found_image(char *image)
{
	if (card_count >= LEN(cards)) {
		fprintf(stderr, "Out of cards: %lu/%lu\n", card_count,
			LEN(cards));
		return ENOBUFS;
	}
	// +1 because we need two spaces.
	if (quiz_len + 1 >= LEN(quiz)) {
		fprintf(stderr, "Out of quiz space: %lu/%lu\n", quiz_len,
			LEN(quiz));
		return ENOBUFS;
	}
	strncpy(cards[card_count].file_name, image,
		STRMAX(cards[card_count].file_name));
	quiz[quiz_len].card_id = card_count;
	quiz[quiz_len].front = 0;
	quiz[quiz_len].confidence = NOT_TESTED;
	quiz[quiz_len].next_review = time(NULL);
	quiz_len++;
	quiz[quiz_len].card_id = card_count;
	quiz[quiz_len].front = 1;
	quiz[quiz_len].confidence = NOT_TESTED;
	quiz[quiz_len].next_review = time(NULL);
	quiz_len++;
	card_count++;

	printf("Loaded image %s.\n", image);

	return 0;
}

int find_image_files(void)
{
	DIR *cwd;
	struct dirent *entry;
	int result = 0;
	struct stat stats;

	cwd = opendir(".");
	if (!cwd) {
		perror("Failed to open current directory.");
		return errno;
	}
	while ((entry = readdir(cwd))) {
		if (stat(entry->d_name, &stats) != 0) {
			fprintf(stderr, "Failed to stat %s: %i.\n",
				entry->d_name, errno);
			continue;
		}
		if (!S_ISREG(stats.st_mode)) {
			printf("%s is not a regular file. Skipping.\n",
				entry->d_name);
			continue;
		}
		if (is_image(entry->d_name)) {
			result = found_image(entry->d_name) != 0;
			if (result != 0) {
				fprintf(stderr, "Failed to add image.\n");
			}
		} else {
			printf("%s not an image.\n", entry->d_name);
		}
	}
	(void)closedir(cwd);

	printf("Loaded %lu/%lu cards, %lu/%lu quiz items.\n", card_count,
		LEN(cards), quiz_len, LEN(quiz));
	return result;
}

int is_image(const char *file)
{
	char *file_types[] = {
		".png",
		".jpg",
		".jpeg",
	};
	int match;
	/*
	 * Loop through every character in the file type looking for the first
	 * character that doesn't match. If everything matched, return nonzero.
	 */
	const size_t name_len = strlen(file);

	for (size_t type = 0; type < LEN(file_types); ++type) {
		const size_t type_len = strlen(file_types[type]);
		if (name_len < type_len) {
			// Not a match.
			continue;
		}
		match = 1;
		const size_t ext_start = name_len - type_len;
		for (size_t i = 0; (i <= type_len) && (match); ++i) {
			if (file[ext_start + i] != file_types[type][i]) {
				match = 0;
			}
		}
		if (match) {
			printf("%s matched %s.\n", file, file_types[type]);
			return 1;
		}
	}
	return 0;
}


/*
 * Replace the variables found in buf with their values.
 */
int replace_in_buf(char *buf, size_t *buf_len, const size_t buf_cap)
{
	size_t dst = 0;
	int result = 0;
	char *var_start;
	struct quiz_item *card;
	const char card_var[] = "$cards";
	const char front_var[] = "$front";
	const char back_var[] = "$back";
	const char card_count_var[] = "$card_count";

	for (; (dst < *buf_len) && (*buf_len < buf_cap) && (result == 0);
			++dst) {
		if (buf[dst] != '$') {
			continue;
		}
		var_start = buf + dst;
		printf("variable dst:%lu buf_len:%lu \"%.20s\"\n", dst,
			*buf_len, var_start);
		card = quiz + current_quiz_item;
		if (memcmp(var_start, card_var, STRMAX(card_var)) == 0) {
			printf("Found cards var.\n");
			result = print_var_to(var_start, buf_len, buf_cap,
				card_var, "%lu", quiz_len);
		} else if (memcmp(var_start, front_var, STRMAX(front_var))
			== 0)
		{
			if (card->front) {
				result = print_var_to(var_start, buf_len,
					buf_cap, front_var,
					"<img src=\"%s\" width=\"400\" height=\"400\">\n",
					cards[card->card_id].file_name);
			} else {
				result = print_var_to(var_start, buf_len,
					buf_cap, front_var, "<p>%s</p>\n",
					cards[card->card_id].file_name);
			}
		} else if (memcmp(var_start, back_var, STRMAX(back_var))
			== 0)
		{
			if (!card->front) {
				result = print_var_to(var_start, buf_len,
					buf_cap, back_var,
					"<img src=\"%s\" width=\"400\" height=\"400\">\n",
					cards[card->card_id].file_name);
			} else {
				result = print_var_to(var_start, buf_len,
					buf_cap, back_var, "<p>%s</p>\n",
					cards[card->card_id].file_name);
			}
		} else if (memcmp(var_start, card_count_var,
				STRMAX(card_count_var)) == 0) {
			result = print_var_to(var_start, buf_len, buf_cap,
				card_count_var, "%lu", quiz_len);
		}
	}
	return result;
}

int show_done_page(int client)
{
	FILE *f = fopen("asl_done.html", "r");
	if (!f) {
		perror("Failed to open file");
		return errno;
	}
	return send_file_with_replaced_params(f, client);
}

int send_file_with_replaced_params(FILE *f, int client)
{
	/* Allocate 4KiB for the file and 2KiB for augmentation by parameter
	 * data. */
	char buf[4 * KIBIBYTE * 2];

	size_t buf_used = fread(buf, 1, sizeof(buf), f);
	if (ferror(f)) {
		perror("Failed to read file.");
		return EIO;
	}
	int result = replace_in_buf(buf, &buf_used, LEN(buf));
	if (result != 0) {
		perror("Failed to replace paramters in file.");
		return result;
	}
	return send_data(client, ok_header, buf, buf_used);
}

void shuffle_cards(void)
{
	for (size_t i = 0; i < quiz_len; ++i) {
		size_t new_pos = ((size_t)rand()) % quiz_len;
		if (new_pos == i)
			continue;
		struct quiz_item tmp = quiz[i];
		quiz[i] = quiz[new_pos];
		quiz[new_pos] = tmp;
	}
}
