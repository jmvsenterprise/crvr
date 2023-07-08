#include <stdlib.h>

struct pool {
	size_t offset = 0;
	size_t cap = 0;
	char *buffer = nullptr;
};

int pool_init(struct pool *p, size_t desired_size);
void pool_free(struct pool *p);
void *pool_alloc(struct pool *p, size_t byte_amount);

#define pool_alloc_type(pool, type)\
	(type*)pool_alloc(pool, sizeof(type))

#define pool_alloc_array(pool, type, count)\
	(type*)pool_alloc(pool, sizeof(type) * count)

