#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int parse_file(FILE *f, const char *file_name);

#if ! _POSIX_C_SOURCE >= 200809L || ! _GNU_SOURCE
size_t strnlen(const char *s, size_t maxlen)
{
	size_t len = 0;
	for (; *s && len < maxlen; s++)
		len++;
	return len;
}
#endif

static int
file_name_to_array_name(char *buf, size_t buf_len, const char *file_name)
{
	size_t index;
	size_t last_slash;
	size_t string_len;

	if (buf_len < strlen(file_name)) {
		fprintf(stderr, "File name \"%s\" too long. %lu bytes max.\n",
			file_name, buf_len);
		return EINVAL;
	}

	memset(buf, 0, buf_len);
	// -1 to keep the null.
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

	return 0;
}

int convert_file(const char *file_name)
{
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
	char *buf = NULL;
	long buf_len = 0;

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

	buf = malloc((size_t)buf_len);
	if (!buf) {
		fprintf(stderr, "Could not allocate buffer: %s\n",
			strerror(errno));
		return ENOMEM;
	}

	size_t bytes_read = fread(buf, 1, (size_t)buf_len, f);
	if (bytes_read != (size_t)buf_len) {
		perror("Failed to read in the whole file!");
		free(buf);
		return EIO;
	}

	char array_name[1024] = {0};
	int error = file_name_to_array_name(array_name, sizeof(array_name),
		file_name);
	if (error) {
		fprintf(stderr, "Failed to parse array name: %i\n", error);
		free(buf);
		return error;
	}

	// Print the array name.
	printf("const char %s[] = {\n", array_name);

	// Read out each line, surrounding them in quotes and indenting by two
	// spaces. To do this, just find newlines and replace them with NULLs
	// so the strings are easy to print.
	for (long i = 0; i < buf_len; ++i) {
		// Print the tab and an opening quote.
		printf("\t\"");
		char *line_start = buf + i;
		for (; (i < buf_len) && (buf[i] != '\n'); ++i) {
			switch (buf[i]) {
			case '"': // fallthrough
			case '\'':
				// Print the string to this point.
				char c = buf[i];
				buf[i] = 0;
				printf("%s", line_start);
				// print an escape.
				putchar('\\');
				// put the char back and continue with the line
				// from here.
				buf[i] = c;
				line_start = buf + i;
				break;
			}
		}
		if (i < buf_len) {
			buf[i] = 0;
		}
		// Print the rest of the line, the closing quote and newline.
		printf("%s\"\n", line_start);
	}

	// Close the array.
	printf("};\n");

	free(buf);

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
