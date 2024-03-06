// SPDX-License-Identifier: BSD-3-Clause

#include "osmem.h"
#include <stdlib.h>
#include "utils.h"

void *os_malloc(size_t size)
{
	if (size == 0)
		return NULL;

	if (META_BLOCK_ALIGNMENT + ALIGN(size) > MMAP_THRESHOLD) {
		// mmap allocation
		struct block_meta *new_block = alloc_meta_block(size);

		return (void *) ((long) new_block + META_BLOCK_ALIGNMENT);
	}

	if (HEAP_NOT_PREALLOCATED) {
		// preallocation permited
		heap_prealloc = 1;
		struct block_meta *new_block = alloc_meta_block(MMAP_THRESHOLD - META_BLOCK_ALIGNMENT);

		new_block->status = STATUS_FREE;
	}

	if (FIRST_BLOCK != NULL)
		coalesce();

	struct block_meta *valid_block = find_best_block(size);

	if (valid_block == NULL) {
		// best block is NOT found
		struct block_meta *last_block = last_block_on_heap();

		if (last_block !=  NULL && last_block->status == STATUS_FREE) {
			// expand last block on heap if it's STATUS_FREE
			struct block_meta *last_block = expand_last_block(size);

			return (void *) ((long) last_block + META_BLOCK_ALIGNMENT);
		}
		// add new block at the final of the list
		struct block_meta *new_block = alloc_meta_block(size);

		return (void *) ((long) new_block + META_BLOCK_ALIGNMENT);
	}

	// best block is found
	if (ALIGN(valid_block->size) - ALIGN(size) >= META_BLOCK_ALIGNMENT + ALIGNMENT)
		// split is possible
		split(valid_block, size);

	// split NOT possible
	valid_block->status = STATUS_ALLOC;

	return (void *) ((long) valid_block + META_BLOCK_ALIGNMENT);
}

void os_free(void *ptr)
{
	if (ptr == NULL)
		return;

	struct block_meta *block = FIRST_BLOCK;

	do {
		if ((long) ptr == (long) block + (long) META_BLOCK_ALIGNMENT) {
			if (block->status == STATUS_ALLOC)
				block->status = STATUS_FREE;

			if (block->status == STATUS_MAPPED) {
				if (block->next == block) {
					// there is a single block in the list
					FIRST_BLOCK = NULL;
					int ret_value = munmap(block, ALIGN(block->size) + META_BLOCK_ALIGNMENT);

					DIE(ret_value < 0, "munmap failed");

					return;
				}

				if (block == FIRST_BLOCK)
					FIRST_BLOCK = block->next;

				block->prev->next = block->next;
				block->next->prev = block->prev;
				int ret_value = munmap(block, ALIGN(block->size) + META_BLOCK_ALIGNMENT);

				DIE(ret_value < 0, "munmap failed");

				return;
			}

			return;
		}
		block = block->next;
	} while (block != FIRST_BLOCK);
}

void *os_calloc(size_t nmemb, size_t size)
{
	if (size == 0 || nmemb == 0)
		return NULL;

	size = size * nmemb;

	if ((long) META_BLOCK_ALIGNMENT + (long) ALIGN(size) > (long) PAGE_SIZE) {
		// mmap allocation
		struct block_meta *new_block = calloc_meta_block(size);

		return (void *) ((long) new_block + META_BLOCK_ALIGNMENT);
	}

	if (HEAP_NOT_PREALLOCATED) {
		// preallocation permited
		heap_prealloc = 1;
		struct block_meta *new_block = alloc_meta_block(MMAP_THRESHOLD - META_BLOCK_ALIGNMENT);

		new_block->status = STATUS_FREE;
	}

	if (FIRST_BLOCK != NULL)
		coalesce();

	struct block_meta *valid_block = find_best_block(size);

	if (valid_block == NULL) {
		// best block is NOT found
		struct block_meta *last_block = last_block_on_heap();

		if (last_block != NULL && last_block->status == STATUS_FREE) {
			// expand last block on heap if it's STATUS_FREE
			struct block_meta *last_block = expand_last_block(size);

			memset((void *) ((long) last_block + META_BLOCK_ALIGNMENT), 0, ALIGN(size));

			return (void *) ((long) last_block + META_BLOCK_ALIGNMENT);
		}
		// add a new block at the final of the list
		struct block_meta *new_block = calloc_meta_block(size);

		return (void *) ((long) new_block + META_BLOCK_ALIGNMENT);
	}

	// best block is NOT found
	if (ALIGN(valid_block->size) - ALIGN(size) >= META_BLOCK_ALIGNMENT + ALIGNMENT) {
		// split is possible
		split(valid_block, size);
		memset((void *) ((long) valid_block + META_BLOCK_ALIGNMENT), 0, ALIGN(size));

		return (void *) ((long) valid_block + META_BLOCK_ALIGNMENT);
	}

	// split NOT possible
	valid_block->status = STATUS_ALLOC;
	memset((void *) ((long) valid_block + META_BLOCK_ALIGNMENT), 0, ALIGN(size));

	return (void *) ((long) valid_block + META_BLOCK_ALIGNMENT);
}

void *os_realloc(void *ptr, size_t size)
{
	if (ptr == NULL) {
		ptr = os_malloc(size);
		return ptr;
	}

	if (size == 0) {
		os_free(ptr);
		return NULL;
	}

	if (FIRST_BLOCK != NULL)
		coalesce();

	struct block_meta *block = FIRST_BLOCK;

	if (block == NULL)
		return NULL;

	do {
		if ((long) block + (long) META_BLOCK_ALIGNMENT == (long) ptr) {
			// ptr is found

			if (block->status == STATUS_FREE)
				return NULL;

			if (ALIGN(size) + META_BLOCK_ALIGNMENT > MMAP_THRESHOLD || block->status == STATUS_MAPPED) {
				void *new_addr = os_malloc(size);

				memmove(new_addr, ptr, MIN(ALIGN(size), ALIGN(block->size)));
				os_free(ptr);

				return new_addr;
			}

			if (ALIGN(size) == ALIGN(block->size)) {
				// new size and old size are the same

				return ptr;
			}

			// block->status == STATUS_ALLOC

			if (ALIGN(size) < ALIGN(block->size)) {
				// new size is SMALLER than the old size
				if (ALIGN(block->size) - ALIGN(size) >= META_BLOCK_ALIGNMENT + ALIGNMENT)
					// split is possible
					split(block, size);

				// split is NOT possible

				return ptr;
			}

			if (block == last_block_on_heap()) {
				// last block cand be expanded
				void *addr = sbrk(ALIGN(size) - ALIGN(block->size));

				DIE(addr == (void *) -1, "sbrk failed");

				block->size = ALIGN(size);

				return ptr;
			}

			if (block->next->status == STATUS_FREE &&
				ALIGN(block->size) + META_BLOCK_ALIGNMENT + ALIGN(block->next->size) >= ALIGN(size) &&
				block->next != FIRST_BLOCK) {
				// the block after the current one is going to be used for reallocation

				block->size = ALIGN(block->size) + META_BLOCK_ALIGNMENT + ALIGN(block->next->size);
				block->next = block->next->next;
				block->next->prev = block;

				if (ALIGN(block->size) - ALIGN(size) >= META_BLOCK_ALIGNMENT + ALIGNMENT)
					// split is possible
					split(block, size);

				// split NOT possible

				return ptr;
			}

			// there is no more space left
			// a new block is allocated at the final of the list and the old one is freed

			void *new_addr = os_malloc(size);

			memmove(new_addr, ptr, ALIGN(block->size));
			os_free(ptr);

			return new_addr;
		}
		block = block->next;
	} while (block != FIRST_BLOCK);

	return NULL;
}
