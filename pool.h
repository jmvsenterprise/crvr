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

struct pool {
	size_t offset;
	size_t cap;
	char *buffer;
};

int pool_init(struct pool *p, size_t desired_size);
void pool_free(struct pool *p);
void *pool_alloc(struct pool *p, size_t byte_amount);
void pool_reset(size_t offset);

#define pool_alloc_type(pool, type)\
	(type*)pool_alloc(pool, sizeof(type))

#define pool_alloc_array(pool, type, count)\
	(type*)pool_alloc(pool, sizeof(type) * count)

#endif
