struct str {
	size_t len;
	char *data;
};

size_t str_find_substr(const struct str *target, const struct str *src)
{
	size_t src_char;
	size_t target_char;
	int found_location = -1;
	int found = 0;
	for (src_char = 0; src_char < src->len; ++src_char) {
		if (src->data[src_char] == target->data[0]) {
			assert(src_char < INT_MAX);
			found_location = (int)src_char;
			found = 1;
			src_char++;
			for (target_char = 1; (target_char < target->len) && (src_char < src->len); ) {
				if (src->data[src_char] != target->data[target_char]) {
					found = 0;
					break;
				}
				src_char++;
				target_char++;
			}
			if (found) {
				return (size_t)found_location;
			}
		}
	}
	return (size_t)-1;
}

int str_cmp(const struct str *a, const struct str *b)
{
	if (a->len > b->len) {
		return -1;
	} else if (a->len < b->len) {
		return 1;
	}
	return strncmp(a->data, b->data, a->len);
}

int str_cmp_chars(const struct str *a, const char *b)
{
	size_t b_len = strlen(b);
	if (a->len > b_len) {
		return -1;
	} else if (a->len < b_len) {
		return 1;
	}
	return strncmp(a->data, b, a->len);
}

int str_alloc(struct str *s, const size_t len, struct pool *p)
{
	assert(s && p);
	if (len == 0) {
		return 0;
	}
	s->len = len;
	s->data = pool_alloc_array(p, char, s->len);
	if (s->data) {
		return 0;
	}
	return ENOMEM;
}

size_t str_cpy(const struct str *src, struct str *dest)
{
	size_t i = 0;
	size_t max = (src->len > dest->len) ? dest->len : src->len;
	for (; i < max; ++i) {
		dest->data[i] = src->data[i];
	}
	return i;
}

void str_print(const struct str *str)
{
	if (str) {
		printf("{%lu, \"%s\"}", str->len, str->data);
	} else {
		printf("{NULL}");
	}
}

struct str_list {
	struct str str;
	struct str_list *next;
};

struct str_list *split(const struct str *src, const struct str *delimiter,
	struct pool *p)
{
	struct str moving_src = *src;
	struct str_list *root = NULL;
	struct str_list *end = NULL;
	struct str_list *next = NULL;
	size_t eow = 0;
	size_t word_size;

	printf("%s: Looking for \"%s\" in \"%s\".\n", __func__, delimiter->data,
		src->data);
	while (eow != (size_t)-1) {
		eow = str_find_substr(delimiter, &moving_src);
		// Create the new str_list.
		next = pool_alloc_type(p, struct str_list);
		if (!next) {
			printf("Failed to allocate str_list.\n");
			return NULL;
		}
		if (eow == (size_t)-1) {
			word_size = moving_src.len + 1;
			printf("No delimiter found. Final word: %s.\n",
				moving_src.data);
		} else {
			word_size = eow;
			printf("Delimiter found at %lu: %s\n", eow,
				moving_src.data + eow);
		}
		printf("Word size is %lu.\n", word_size);
		if (str_alloc(&next->str, word_size, p) == ENOMEM) {
			printf("Failed to allocate next's str.\n");
			return NULL;
		}
		memcpy(next->str.data, moving_src.data, next->str.len);
		printf("next is {%lu, \"%s\"}.\n", next->str.len,
			next->str.data);

		// Advance our src pointer now that we've copied the data.
		moving_src.data += word_size + 1;
		moving_src.len -= word_size + 1;
		printf("Remaining src: \"%s\". Len=%lu.\n", moving_src.data,
			moving_src.len);

		// Attach the str_list to the list of str_lists.
		if (end) {
			end->next = next;
			printf("Updated end->next: {%lu, %s}\n",
				end->next->str.len, end->next->str.data);
			end = next;
		} else if (root) {
			end = root->next = next;
			printf("Updated root->next: {%lu, %s}\n",
				root->next->str.len, root->next->str.data);
		} else {
			end = root = next;
			printf("Updated end: {%lu, %s}\n", end->str.len,
				end->str.data);
		}
	}
	printf("%s: Done splitting. Final list:\n", __func__);
	next = root;
	while (next) {
		str_print(&next->str);
		printf("\n");
		next = next->next;
	}
	return root;
}
