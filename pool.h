/*
 * Copyright (C) Joseph M Vrba 2023
 *
 * This file defines a pool structure and associated routines. The pool
 * manages a memory pool that is allocated from the OS and can be used for
 * dynamic memory allocations without going all the way to the OS.
 *
 * The pool is designed to be created and reused over and over again. First
 * initialize the pool to a large size, then allocate items from it. When
 * you are done with those items, just call pool_reset to "free" them, which
 * just tells the pool to start allocating from the provided index again.
 * 
 * This makes deallocation constant time.
 */
#ifndef POOL_H
#define POOL_H

/*
 * Stores a block of memory, the offset into it where memory is unused and
 * the total capacity of the block.
 */
struct pool {
	long offset;
	long cap;
	char *buffer;
};

/*
 * Initialize the memory pool.
 *
 * p - The pool to initialize.
 * desired_size - The capacity of the pool.
 *
 * Returns 0 if the pool, p, was successfully initialized to the desired
 * size. Otherwise returns an error code.
 */
int pool_init(struct pool *p, unsigned long desired_size);

/**
 * @brief Return the number of unused bytes in the pool.
 *
 * @param[in] p - The pool to query.
 *
 * @return Returns the space left in p. Returns -1 if p is NULL.
 */
long pool_get_remaining_capacity(struct pool *p)

/*
 * Free all memory allocated to the pool.
 *
 * WARNING: Using any objects allocated as part of the pool after this call
 * may lead to undefined behavior as the block of memory in the pool will be
 * returned to the OS.
 *
 * p - The pool to free.
 */
void pool_free(struct pool *p);

/*
 * Allocate memory from the pool and return it.
 *
 * p - The pool to allocate from.
 * byte_amount - The number of bytes to allocate.
 *
 * Returns a valid pointer if successful. If the pool has no more memory
 * NULL will be returned.
 */
void *pool_alloc(struct pool *p, long byte_amount);

/**
 * @brief Returns the current position of the pool.
 *
 * The value returned can be used to reset the pool to an earlier location,
 * effectively freeing everything that happened after the location was obtained.
 *
 * @param[in] p - The pool to get the value from.
 *
 * @return Returns the pool's current position. Returns -1 if p is NULL.
 */
long pool_get_position(struct pool *p);

/**
 * @brief Reset the pool to a specific offset, reclaiming memory.
 *
 * If one saves the pool's offset to a variable, one can call pool_alloc
 * after that and it will allocate memory above that offset. Sending that
 * offset into this function will reset the pool to the provided offset,
 * freeing the memory allocated after the offset was saved.
 *
 * This can be used to allocate portions of the pool for specific jobs and
 * then get rid of all the memory in constant time. Pass in an offset of
 * 0 to reclaim the entire pool for reuse.
 *
 * @param[in,out] p - The pool to reset.
 * @param[in] offset - The offset to set the pool to.
 *
 * @return Returns 0 if the pool was reset successfully. Otherwise returns an
 *         error code.
 */
int pool_reset(struct pool *p, long offset);

/*
 * Helper macro to allocate memory for a specific type of object.
 *
 * pool - The pool to allocate from.
 * type - The type of object to allocate.
 *
 * Returns the allocated memory if successful, otherwise returns NULL.
 */
#define pool_alloc_type(pool, type)\
	(type*)pool_alloc(pool, sizeof(type))

/*
 * Helper macro to allocate memory for an array of specific objects.
 *
 * pool - The pool to allocate from.
 * type - The type of object to allocate.
 * count - The number of objects of the specified type to allocate.
 *
 * Returns the allocated memory if successful, otherwise returns NULL.
 */
#define pool_alloc_array(pool, type, count)\
	(type*)pool_alloc(pool, sizeof(type) * count)

/*
 * If you set this #define, the functions definitions will be included in the
 * current file.
 */
#ifdef DEFINE_POOL

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

int pool_init(struct pool *p, unsigned long desired_size)
{
	assert(p && (p->buffer == NULL));
	p->offset = 0;
	p->cap = desired_size;
	p->buffer = malloc((size_t)p->cap);

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

void *pool_alloc(struct pool *p, long byte_amount)
{
	unsigned long alignment = sizeof(void*) - byte_amount % sizeof(void*);
	byte_amount += alignment;
	if ((p->offset + byte_amount) > p->cap) {
		return NULL;
	}
	assert(p->buffer);
	void *allocation = (void*)(p->buffer + p->offset);
	p->offset += byte_amount;
	return allocation;
}

long pool_get_position(struct pool *p)
{
	if (!p) return -1;
	return p->offset;
}

int pool_reset(struct pool *p, long offset)
{
	if (!p || (offset < 0)) return EINVAL;
	p->offset = offset;
	return 0;
}

long pool_get_remaining_capacity(struct pool *p)
{
	if (!p) return -1;
	return p->cap - p->offset;
}

#endif // DEFINE_POOL

#endif
