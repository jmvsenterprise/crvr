/**
 * @file
 *
 * Copyright (C) 2023 Joseph M Vrba
 *
 * This file declares the str struct and functions that manipulate it. The str
 * is the only string you'll ever need. Dynamic allocation should be handled by
 * a more efficient structure like a buffer anyway. I recommend the pool.
 *
 * The str stores a pointer to the data it refers to and the "length" of the
 * string, or the number of characters in the data it refers to as being part of
 * the string.
 */
#ifndef BASE_STR_H
#define BASE_STR_H

#include <stdio.h>

struct pool;

struct str {
	char *s;
	long len;
};

#define STR(string) (struct str){string, (sizeof(string) / sizeof(string[0]))}

/*
 * Compare two strs.
 *
 * a - The first str.
 * b - The second str.
 *
 * Returns -1 if a < b, 0 if they are the same string, and 1 if a > b.
 */
int str_cmp(const struct str *a, const struct str *b);

/*
 * Compare a str against a c-string.
 *
 * s - The str.
 * cs - The c-string.
 *
 * Returns -1 if s < cs, 0 if they contain the same data, and 1 if s > cs.
 */
int str_cmp_cstr(const struct str *s, const char *cs);

/*
 * Search for needle in haystack, and return needle's starting index.
 *
 * haystack - The string to search for needle in.
 * needle - The string to search for.
 *
 * Returns the index where needle starts, or -1 if needle wasn't found.
 */
long str_find_substr(const struct str *haystack, const struct str *needle);

/*
 * Prints the str to the provided file.
 *
 * f - The file to print to.
 * s - The str to print.
 *
 * Returns 0 if s was printed successfully. Otherwise returns an error code.
 */
int str_print(FILE *f, const struct str *s);

/**
 * @brief Creates a buffer in the pool and stores it in a str.
 *
 * This function creates a buffer of the requested length in the pool and
 * contains it in an str.
 *
 * @param[in,out] p - The pool to allocate the buffer in.
 * @param[in] space_needed - The size the buffer should be in bytes.
 * @param[out] s - The str to store the buffer to.
 *
 * @return Returns 0 if successful, otherwise returns an error code.
 */
int alloc_str(struct pool *p, const unsigned long space_needed, struct str *s);

/**
 * @brief Copies the c-string into an str.
 *
 * Copies the c-string data into a buffer on the pool and updates a provided str
 * to contain that data. The data for the str remains in the pool.
 *
 * @param[in,out] p - The pool to allocate the buffer in.
 * @param[in] cstr - The c-string to copy into the pool.
 * @param[in] len - The length of the cstr to copy.
 * @param[in,out] s - The str to store the data to.
 *
 * @return Returns 0 if the data was copied to the pool and the str was updated.
 *         Otherwise returns an error code.
 */
int alloc_from_str(struct pool *p, const char *cstr, const long len,
	struct str *s);


#ifdef DEFINE_STR

#include <string.h>
#include "pool.h"

int str_cmp(const struct str *a, const struct str *b)
{
  if (a == b)
    return 0;
  for (long i = 0; i < a->len; ++i) {
    for (long j = 0; j < b->len; ++j) {
      if (a->s[i] < b->s[i]) {
        return -1;
      } else if (a->s[i] > b->s[i]) {
        return 1;
      }
    }
  }
  return 0;
}

int str_cmp_cstr(const struct str *s, const char *cs)
{
  const size_t len = strlen(cs);
  for (long i = 0; i < s->len; ++i) {
    for (size_t j = 0; j < len; ++j) {
      if (s->s[i] < cs[j]) {
        return -1;
      } else if (s->s[i] > cs[j]) {
        return 1;
      }
    }
  }
  return 0;
}

/*
 * I feel like there is a faster way to write this...
 */
long str_find_substr(const struct str *haystack, const struct str *needle)
{
	long i = 0;
	long j;
	int found = 0;
	for (; i < haystack->len; ++i) {
		found = 1;
		for (j = 1; (i + j < haystack->len) && (j < needle->len); ++j) {
			if (haystack->s[i + j] != needle->s[j]) {
				// Skip characters we scanned.
				i += j;
				found = 0;
				break;
			}
		}
		if (found) {
			return i;
		}
	}
	return -1;
}

int str_print(FILE *f, const struct str *s)
{
  const char end = s->s[s->len];
  s->s[s->len] = '\0';
  int result = fputs(s->s, f);
  s->s[s->len] = end;
  return result;
}

int alloc_str(struct pool *p, const long space_needed, struct str *s)
{
	if (!p || !s || (space_needed == 0)) {
		return EINVAL;
	}
	s->s = pool_alloc(p, space_needed);
	if (!s->s) {
		return ENOMEM;
	}
	s->len = space_needed;
	return 0;
}

int alloc_from_str(struct pool *p, const char *cstr, const long len,
	struct str *s)
{
	int alloc_error = alloc_str(p, len, s);
	if (alloc_error) {
		return alloc_error;
	}
	(void)memcpy(s->s, cstr, s->len);
	return 0;
}

#endif // DEFINE_STR

#endif // BASE_STR_H
