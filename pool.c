#include "pool.h"

#include <assert.h>
#include <errno.h>

int pool_init(struct pool *p, size_t desired_size)
{
	assert(p && (p->buffer == NULL));
	p->offset = 0;
	p->cap = desired_size;
	p->buffer = (char*)malloc(p->cap);

	if (!p->buffer) {
		return errno;
	}
	return 0;
}

void pool_free(struct pool *p)
{
	if (p && p->buffer) {
		free(p->buffer);
		p->buffer = NULL;
	}
	p->offset = p->cap = 0;
}

void *pool_alloc(struct pool *p, size_t byte_amount)
{
	size_t alignment = sizeof(void*) - byte_amount %
		sizeof(void*);
	byte_amount += alignment;
	if ((p->offset + byte_amount) > p->cap) {
		return NULL;
	}
	assert(p->buffer);
	void *allocation = (void*)(p->buffer + p->offset);
	p->offset += byte_amount;
	return allocation;
}
