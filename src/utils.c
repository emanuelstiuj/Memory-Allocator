// SPDX-License-Identifier: BSD-3-Clause

#include "utils.h"
#include <stdlib.h>

int heap_prealloc;
struct block_meta *start_block_ptr;

struct block_meta *calloc_meta_block(size_t size)
{
	void *addr;
	int status;

	if ((long) ALIGN(size) + (long) META_BLOCK_ALIGNMENT <= (long) PAGE_SIZE) {
		addr = sbrk(META_BLOCK_ALIGNMENT + ALIGN(size));
		DIE(addr == (void *) -1, "sbrk failed");
		status = STATUS_ALLOC;
	} else {
		addr = mmap(NULL, ALIGN(size) + META_BLOCK_ALIGNMENT, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
		DIE(addr == (void *) -1, "mmap failed");
		status = STATUS_MAPPED;
	}

	struct block_meta *new_block = (struct block_meta *) addr;

	new_block->size = ALIGN(size);
	new_block->status = status;
	memset((void *)((long) new_block + META_BLOCK_ALIGNMENT), 0, ALIGN(size));
	add_new_block(new_block);

	return new_block;
}

struct block_meta *alloc_meta_block(size_t size)
{
	void *addr;
	int status;

	if (ALIGN(size) + META_BLOCK_ALIGNMENT <= MMAP_THRESHOLD) {
		addr = sbrk(META_BLOCK_ALIGNMENT + ALIGN(size));
		DIE(addr == (void *) -1, "sbrk failed");
		status = STATUS_ALLOC;
	} else {
		addr = mmap(NULL, ALIGN(size) + META_BLOCK_ALIGNMENT, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
		DIE(addr == (void *) -1, "mmap failed");
		status = STATUS_MAPPED;
	}

	struct block_meta *new_block = (struct block_meta *) addr;

	new_block->size = ALIGN(size);
	new_block->status = status;
	add_new_block(new_block);

	return new_block;
}

struct block_meta *find_best_block(size_t size)
{
	struct block_meta *curr_block = FIRST_BLOCK;
	struct block_meta *best_block = NULL;
	int smallest_diff = __INT32_MAX__;

	if (curr_block == NULL)
		return NULL;

	do {
		if (curr_block->size >= size && curr_block->status == STATUS_FREE &&
			(long) curr_block->size - (long) size < (long) smallest_diff) {
			smallest_diff = curr_block->size - size;
			best_block = curr_block;
		}
		curr_block = curr_block->next;
	} while (curr_block != FIRST_BLOCK);

	return best_block;
}

void add_new_block(struct block_meta *new_block)
{
	if (FIRST_BLOCK == NULL) {
		new_block->next = new_block;
		new_block->prev = new_block;
		FIRST_BLOCK = new_block;

		return;
	}

	new_block->next = FIRST_BLOCK;
	new_block->prev = LAST_BLOCK;
	new_block->prev->next = new_block;
	new_block->next->prev = new_block;
}

void coalesce(void)
{
	struct block_meta *curr_block = FIRST_BLOCK;

	while (curr_block != LAST_BLOCK) {
		if (curr_block->status == STATUS_FREE && curr_block->next->status == STATUS_FREE) {
			int total_size = ALIGN(curr_block->size) + META_BLOCK_ALIGNMENT + ALIGN(curr_block->next->size);

			curr_block->size = total_size;
			curr_block->next = curr_block->next->next;
			curr_block->next->prev = curr_block;
			curr_block = curr_block->prev;
		}
		curr_block = curr_block->next;
	}
}

void split(struct block_meta *block, int new_block_size)
{
	struct block_meta *second_block = (struct block_meta *) ((long) block + META_BLOCK_ALIGNMENT + ALIGN(new_block_size));

	second_block->size = ALIGN(block->size) - ALIGN(new_block_size) - META_BLOCK_ALIGNMENT;
	second_block->status = STATUS_FREE;

	block->size = ALIGN(new_block_size);
	block->status = STATUS_ALLOC;

	second_block->next = block->next;
	second_block->prev = block;
	second_block->prev->next = second_block;
	second_block->next->prev = second_block;
}

struct block_meta *expand_last_block(int size)
{
	struct block_meta *last_heap_block = last_block_on_heap();
	void *addr = sbrk(ALIGN(size) - ALIGN(last_heap_block->size));

	DIE(addr == (void *) -1, "sbrk failed");

	last_heap_block->size = ALIGN(size);
	last_heap_block->status = STATUS_ALLOC;

	return last_heap_block;
}

struct block_meta *last_block_on_heap(void)
{
	struct block_meta *block = LAST_BLOCK;

	do {
		if (block->status == STATUS_ALLOC || block->status == STATUS_FREE)
			return block;

		block = block->prev;
	} while (block != LAST_BLOCK);

	return NULL;
}
