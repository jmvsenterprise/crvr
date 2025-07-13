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

#include <math.h>
#include <limits.h>
#include <stdio.h>

/// The End Of the STR. Used for getting substrings.
#define EOSTR (-1)

struct pool;

struct str {
	char *s;
	long len;
};

struct dstr {
	char *s;
	long len;
	long cap;
};

/* -1 for \0 */
#define STR(string) (struct str){\
	string,\
	(sizeof(string) / sizeof(string[0])) - 1\
}

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

/**
 * @brief Get a substring from an str.
 *
 * Its error prone to extract a substring manually from an str, so this is a
 * helper function to do that.
 *
 * @param[in] original - The original str to pull the substring from.
 * @param[in] start - The location to start the substring.
 * @param[in] end - The location to end the substring. If this is EOSTR, then
 *                  the remaining length of original after start will be
 *                  included in the substr.
 * @param[out] substr - The location to store the substr.
 *
 * @return Returns 0 if the substr was extracted successfully. Otherwise returns
 *         and error code.
 */
int str_get_substr(const struct str *original, const long start, const long end,
	struct str *substr);

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
int str_alloc(struct pool *p, const long space_needed, struct str *s);

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
int str_alloc_from_cstr(struct pool *p, const char *cstr, const long len,
	struct str *s);

/**
 * @brief Converts the contents of an str to a long.
 *
 * @param[in] s - The str to attempt to convert.
 * @param[in] base - The base to use when converting the string representation
 *                   to the number.
 * @param[out] l - The location to store the long values.
 *
 * @return Returns 0 if the conversion was successful and l is populated.
 *         Otherwise returns an error code.
 */
int str_to_long(const struct str *s, long base, long *l);

/**
 * @brief Copies the str data into a c-string.
 *
 * @param[in] s - The str to copy data from.
 * @param[out] dest - The pre-allocated character array to save data to.
 * @param[in] dest_len - The length of the array at dest, which will be checked
 *                       to avoid exceeding the buffer length.
 *
 * @return Returns 0 if the data was successfully copied. Otherwise returns an
 *         error code.
 */
int str_copy_to_cstr(const struct str *s, char *dest, long dest_len);

/**
 * @brief Frees the memory used by the dstr.
 *
 * Note: This assumes the dstr was allocated using malloc.
 *
 * @param[in] ds - The dstr to free. If it is null, then nothing is done. If
 *                 the dstr contains null pointers, then nothing is done.
 */
void dstr_free(struct dstr *ds);

/**
 * @brief Append the str data to the dstr, allocating memory in the dstr as
 *        needed.
 *
 * @param[in,out] ds - The dstr to copy the data into.
 * @param[in] s - The str to get data from.
 *
 * @return Returns zero if the dstr was successfully appended to. Otherwise
 *         returns an error code.
 */
int dstr_append_str(struct dstr *ds, const struct str *s);

/**
 * @brief Append a C-string to the dstr, allocating memory in the dstr as
 *        needed.
 *
 * @param[in,out] ds - The dstr to copy the data into.
 * @param[in] cstr - The C-string to copy data from. Data will be copied up to
 *                   the NULL terminator in the string.
 *
 * @return Returns 0 if the data was successfully copied into the dstr.
 *         Otherwise, an error code is returned.
 */
int dstr_append_cstr(struct dstr *ds, const char *cstr);
 
/**
 * @brief Insert a str into the dstr at the index.
 *
 * @param[in,out] ds - The dstr to insert the string into.
 * @param[in] index - The location in the dstr to insert the cstr. All data in
 *   the dstr after this location will be pushed out to make room for the cstr.
 * @param[in] str - The str to insert into the dstr.
 *
 * @return Returns 0 if the cstr was inserted successfully. Otherwise an error
 *         code is returned.
 */
int dstr_insert_str(struct dstr *ds, long index, const struct str *str);

/**
 * @brief Insert a C-string into the dstr at the index.
 *
 * @param[in,out] ds - The dstr to insert the string into.
 * @param[in] index - The location in the dstr to insert the cstr. All data in
 *   the dstr after this location will be pushed out to make room for the cstr.
 * @param[in] cstr - The cstr to insert into the dstr.
 *
 * @return Returns 0 if the cstr was inserted successfully. Otherwise an error
 *         code is returned.
 */
int dstr_insert_cstr(struct dstr *ds, long index, const char *cstr);

#ifdef DEFINE_STR

#include <string.h>
#include "pool.h"

#define LONG_9 ((long)'9' - (long)'0')

int str_cmp(const struct str *a, const struct str *b)
{
	long i;
	long len = a->len;
	if (a == b) return 0;
	// Is one bigger than the other? In that case, only compare values to
	// the smallest number.
	if (b->len < len) {
		len = b->len;
	}
	for (i = 0; i < len; ++i) {
		if (a->s[i] < b->s[i]) return -1;
		if (a->s[i] > b->s[i]) return 1;
	}
	// If the lengths didn't match, return which one was smaller.
	if (a->len == b->len) return 0;
	if (a->len < b->len) return -1;
	return 1;
}

int str_cmp_cstr(const struct str *s, const char *cs)
{
	size_t unsigned_len = strlen(cs);
	if (unsigned_len > LONG_MAX) return ERANGE;
	long len = (long)unsigned_len;
	long i;
	for (i = 0; i < len; ++i) {
		if (s->s[i] < cs[i]) return -1;
		if (s->s[i] > cs[i]) return 1;
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
	char left;
	char right;
	for (; i < haystack->len; ++i) {
		found = 1;
		for (j = 0; (i + j < haystack->len) && (j < needle->len); ++j) {
			left = haystack->s[i + j];
			right = needle->s[j];
			if (left != right) {
				// Skip characters we scanned.
				i += j;
				found = 0;
				break;
			}
		}
		if (found) return i;
	}
	return -1;
}

int str_get_substr(const struct str *original, const long start, const long end,
	struct str *substr)
{
	if (!original || (start < 0) || (start > original->len) || !substr) {
		return EINVAL;
	}
	// end can be EOSTR, but if it isn't it needs to be valid.
	if ((end != EOSTR) && ((end < 0) || (end < start) ||
		(end > original->len)))
	{
		return EINVAL;
	}
	substr->s = original->s + start;
	if (end == EOSTR)
		substr->len = original->len - start;
	else
		substr->len = end - start;
	return 0;
}

int str_print(FILE *f, const struct str *s)
{
	if (!f || !s) return EINVAL;
	for (long i = 0; i < s->len; ++i) {
		fputc(s->s[i], f);
	}
	return 0;
}

int str_alloc(struct pool *p, const long space_needed, struct str *s)
{
	if (!p || !s || (space_needed <= 0)) return EINVAL;

	s->s = pool_alloc(p, space_needed);
	if (!s->s) return ENOMEM;
	s->len = space_needed;
	return 0;
}

int str_alloc_from_cstr(struct pool *p, const char *cstr, const long len,
	struct str *s)
{
	int err = str_alloc(p, len, s);
	if (err) return err;
	assert(s->len > 0);
	(void)memcpy(s->s, cstr, (size_t)s->len);
	return 0;
}


int str_to_long(const struct str *s, long base, long *l)
{
	if (!s || !l || (base <= 0)) return EINVAL;

	const double max_value = round(pow((double)base, (double)s->len));
	if (max_value > (double)LONG_MAX) return ERANGE;
	long value = 0;
	long c = 0;
	for (long i = 0; i < s->len; ++i) {
		c = s->s[i];
		c -= '0';
		if (c > LONG_9) return ERANGE;
		value *= base;
		value += c;
	}
	*l = value;
	return 0;
}

int str_copy_to_cstr(const struct str *s, char *dest, long dest_len)
{
	if (!s || !dest || (dest_len <= 0)) return EINVAL;
	if (s->len >= dest_len) return ENOSPC;

	assert(s->len > 0);
	memcpy(dest, s->s, (size_t)s->len);
	dest[s->len + 1] = 0;
	return 0;
}

void
dstr_free(struct dstr *ds)
{
	if (!ds) return;
	if (ds->s) free(ds->s);
	ds->len = ds->cap = 0;
}

static int
dstr_grow_by(struct str *ds, long bytes)
{
	long new_cap;
	char *new_s;

	errno = 0;
	if (ds->cap == LONG_MAX) {
		return ENOMEM;
	}
	new_cap = ds->cap + bytes;
	if (new_cap < 0) {
		// Can't fit the string in.
		return ENOBUFS;
	}
	new_s = calloc(new_cap, sizeof(*new_s));
	if (!new_s) {
		return errno? errno: ENOMEM;
	}
	memcpy(new_s, ds->s, ds->len);
	free(ds->s);
	ds->cap = new_cap;
}

int
dstr_append_str(struct dstr *ds, const struct str *s)
{
	long space;
	int error;
	int i;

	if (!ds || !s) return EINVAL;
	space = ds->cap - ds->len;
	if (space < s->len) {
		error = dstr_grow_by(ds, s->len);
		if (error) return error;
	}
	for (i = ds->len; (i < s->len) && (i < ds->cap); ++i) {
		ds->s[i] = s->s[i];
	}
	ds->len = i;
	return 0;
}

int
dstr_append_cstr(struct dstr *ds, const char *cstr)
{
	if (!ds || !cstr) return EINVAL;
	struct str s = {.s = cstr, .len = strlen(cstr)};
	return dstr_append_str(ds, &s);
}

int
dstr_insert_str(struct dstr *ds, long index, const struct str *str)
{
	long space;

	if (!ds || !s || (index >= ds->cap) return EINVAL;
	space = ds->cap - ds->len;
	if (space < str->len) {
		error = dstr_grow_by(ds, str->len);
		if (error) return error;
	}
	memmove(ds->s + index + str->len, ds->s + index, str->len);
	memcpy(ds->s + index, str->s, str->len);
	return 0;
}

int
dstr_insert_cstr(struct dstr *ds, long index, const char *cstr)
{
	if (!ds || (index >= ds->cap) || !cstr) return EINVAL;
	struct str s = {.s = cstr, .len = strlen(cstr)};
	return dstr_insert_str(ds, index, &s);
}

#endif // DEFINE_STR

#endif // BASE_STR_H
