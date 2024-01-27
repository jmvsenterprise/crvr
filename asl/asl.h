/*
 * Copyright (C) 2023 Joseph M Vrba
 *
 * This file contains structures and function declarations for the American
 * Sign Language web application for crvr.
 */
#ifndef ASL_H
#define ASL_H

#include "http.h"

#include <stdio.h>
#include <time.h>

/*
 * The card represents an object with a front and back. Right now it only
 * supports that one side is an image and the other side is the text of
 * that image.
 */
struct card {
	char file_name[FILENAME_MAX];
};

/*
 * The quiz_item is an entry in the quiz, which corresponds to a card and
 * whether the front or back of the card is shown first. Users are quizzed
 * on both sides of the card, so there are two quiz_items per card.
 */
struct quiz_item {
	size_t card_id;
	int front;
	int confidence;
	time_t next_review;
};

/*
 * Initialize the ASL application.
 *
 * Returns 0 if the application initialized successfully. Otherwise returns an
 * error code.
 */
int asl_init(void);

/*
 * Read the file into the buffer, but swap out the variables with the current
 * values.
 */
int asl_get(struct request *r, int client);

/*
 * This routine handles a POST request to the ASL application.
 *
 * r - The request to handle
 * client - The client to respond to after handling the post request.
 *
 * Returns 0 if the post was handled, and an error code if it fails.
 */
int asl_post(struct request *r, int client);

#endif // ASL_H
