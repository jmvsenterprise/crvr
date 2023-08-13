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

#include <stdlib.h>

/*
 * Stores a block of memory, the offset into it where memory is unused and
 * the total capacity of the block.
 */
struct pool {
	size_t offset;
	size_t cap;
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
int pool_init(struct pool *p, size_t desired_size);

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
void *pool_alloc(struct pool *p, size_t byte_amount);

/*
 * Reset the pool to a specific offset, reclaiming memory.
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
 * p - The pool to reset.
 * offset - The offset to set the pool to.
 */
void pool_reset(struct pool *p, size_t offset);

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

#endif
