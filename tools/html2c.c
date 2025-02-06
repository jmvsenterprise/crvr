#include <stdio.h>

int main(int argc, char **argv)
{
	int result = 0;
	for (int i = 1; (result == 0) && (i < argc); ++i) {
		result = convert_file(argv[i]);
	}
	return result;
}
