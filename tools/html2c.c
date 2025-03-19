#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#if ! _POSIX_C_SOURCE >= 200809L || ! _GNU_SOURCE
size_t strnlen(const char *s, size_t maxlen)
{
	size_t len = 0;
	for (; *s && len < maxlen; s++)
		len++;
	return len;
}
#endif

static void
file_name_to_array_name(char *buf, size_t buf_len, const char *file_name)
{
	size_t index;
	size_t last_slash;
	size_t string_len;

	memset(buf, 0, buf_len);
	(void)strncpy(buf, file_name, buf_len - 1);
	string_len = strnlen(buf, buf_len);

	// Find the last '/' in the path.
	last_slash = buf_len;
	for (index = 0; index < string_len; ++index) {
		if (buf[index] == '/') {
			last_slash = index;
		}
	}
	if (last_slash < buf_len) {
		last_slash++;
		// Found a slash, so move everything in the buffer forward.
		memmove(buf, &buf[last_slash], string_len - last_slash);
		string_len -= last_slash;
		buf[string_len] = 0;
	}
	for (index = 0; index < string_len; ++index) {
		// Change spaces to '_', change '.' to '_'.
		if ((buf[index] == '.') || (buf[index] == ' ')) {
			buf[index] = '_';
		}
	}
}

int convert_file(const char *file_name)
{
	char *buf = NULL;
	long buf_len = 0;
	size_t eol = 0;
	size_t line_start = 0;
	size_t bytes_read;
	size_t bytes_written;
	FILE *f;
	int exit_code = 0;

	// Open the file, figure out how big it is and read it all in.
	f = fopen(file_name, "rb");
	if (f) {
		exit_code = parse_file(f, file_name);
		fclose(f);
	} else {
		return errno? errno : EINVAL;
	}

	return exit_code;
}

int parse_file(FILE *f, const char *file_name)
{
	if (fseek(f, 0, SEEK_END) < 0) {
		perror("Failed to seek in file");
		return EIO;
	}
	buf_len = ftell(f);
	if (buf_len < 0) {
		perror("Failed to get file length");
		return EIO;
	}
	rewind(f);

	buf = malloc(buf_len);
	if (!buf) {
		fprintf(stderr, "Could not allocate buffer: %s\n",
			strerror(errno));
		return ENOMEM;
	}

	size_t bytes_read = fread(buf, 1, buf_len, f);
	if (bytes_read != (size_t)buf_len) {
		perror("Failed to read in the whole file!");
		return EIO;
	}

	file_name_to_array_name(buf, sizeof(buf), file_name);

	// Print the array name.
	printf("const char %s[] = \n", buf);

	// Read out each line, surrounding them in quotes and indenting by two
	// spaces.
	for (long i = 0; i < buf_len; ++i) {
		printf("\t\"");
		long line_start = i;
		for (; (i < buf_len) && (buf[i] != '\n'); ++i);
		printf("\"%.*s\"", i - line_start, buf + line_start);
		// Skip past the \n.
		i++
	}

	// Close the array.
	printf("};\n");

	return 0;
}

int main(int argc, char **argv)
{
	int result = 0;
	for (int i = 1; (result == 0) && (i < argc); ++i) {
		result = convert_file(argv[i]);
	}
	return result;
}
