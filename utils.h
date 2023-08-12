/*
 * Copyright (C) 2023 Joseph M Vrba
 *
 * This file contains useful defines and functions that are generally, or not,
 * useful.
 */
#ifndef UTILS_H
#define UTILS_H

#define KILOBYTE (1000)
#define KIBIBYTE (1024)
#define MEGABYTE (1000000)
#define MEBIBYTE (KIBIBYTE * KIBIBYTE)
#define GIGABYTE (1000000000)
#define GIBIBYTE (MIBIBYTE * MIBIBYTE)

#define SECONDS_PER_DAY (60 * 60 * 24)

#define LEN(p) (sizeof(p) / sizeof(p[0]))
#define STRMAX(p) (LEN(p) - 1)

/*
 * Load the contents of a file into a buffer.
 *
 * file_name - The path to the file to load.
 * buffer - The location to store the file contents.
 * buf_len - The maximum length of the buffer.
 * bytes_loaded - This will be set to how many bytes of the file were read
 *                into the buffer.
 *
 * Returns zero on success and an error code on failure.
 */
int load_file(const char *file_name, char *buffer, const size_t buf_len,
	size_t *bytes_loaded);

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
int print_var_to(char *buf, size_t *buf_len, const size_t buf_cap,
	const char *var_name, const char *format, ...);

#endif // UTILS_H
