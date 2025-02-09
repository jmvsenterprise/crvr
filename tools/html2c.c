#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static void
file_name_to_array_name(char *buf, size_t buf_len, const char *file_name)
{
	size_t index;
	size_t last_slash;
	size_t string_len;

	memset(buf, 0, buf_len);
	string_len = strlcpy(buf, file_name, buf_len - 1);

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
		// Escape quote characters
		if (buf[index] == '"') {
			// Check that we aren't out of buffer space.
			assert((index + 1 + string_len) > buf_len);
			memmove(&buf[index + 1], &buf[index],
				string_len - index);
			buf[index] = '\\';
			// Increment the index so it points at the '"' again.
			// When we loop it'll move past it.
			index++;
			string_len++;
			continue;
			#error expected to see escaped quotes, but not seeing it.
		}
	}
}

int convert_file(const char *file_name)
{
	char buf[4096];
	size_t bytes_to_read;
	size_t eol;
	size_t line_start = 0;
	size_t bytes_read;
	size_t bytes_written;
	FILE *f;

	f = fopen(file_name, "rb");
	if (!f) {
		return errno? errno : EINVAL;
	}

	file_name_to_array_name(buf, sizeof(buf), file_name);

	// Print the array name and the very first quote.
	printf("const char %s[] = \n\"", buf);
	bytes_to_read = sizeof(buf);
	while (!feof(f) && !ferror(f)) {
		bytes_read = fread(buf, 1, bytes_to_read, f);
		if (!bytes_read) continue;

		// Loop through the buffer until we get to the end. Everytime
		// we find a \n, print the whole line, end it with a quote and
		// new line. Then start the next line with its opening quote.
		line_start = 0;
		for (eol = 0; eol < bytes_read; ++eol) {
			if (buf[eol] == '\n') {
				// Print the line.
				bytes_written = fwrite(&buf[line_start], 1,
					eol - line_start, stdout);
				if (bytes_written != (eol - line_start)) {
					printf("Failed to write %lu bytes. Wrote %lu bytes. Errno=%i\n",
						(eol - line_start),
						bytes_written, errno);
					return EIO;
				}
				printf("\"\n\"");
				line_start = eol + 1;
				continue;
			}
		}

		/*
		 * The current line might be split by the end of the buffer. If
		 * so, move the current line to the front of the buffer, then
		 * read in more from the file to get the next line.
		 */
		if (line_start < eol) {
			// Yeah need to preserve the data from the current line.
			// Move it to the front.
			memmove(buf, &buf[line_start], eol - line_start);
			// Reset the indexes since we moved the data.
			eol = eol - line_start;
			line_start = 0;
			// Read in data to fill the buffer.
			bytes_to_read = sizeof(buf) - eol;
		}
	}

	// Print the final line and quote.
	if (eol != line_start) {
		bytes_written = fwrite(&buf[line_start], 1, eol - line_start,
			stdout);
		if (bytes_written != (eol - line_start)) {
			printf("Failed to write %lu bytes. Wrote %lu bytes. Errno=%i\n",
				(eol - line_start), bytes_written, errno);
			return EIO;
		}
	}

	fclose(f);

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
