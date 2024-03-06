#ifndef _UTILS_H_
#define _UTILS_H_

#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "block_meta.h"

extern int heap_prealloc;
extern struct block_meta *start_block_ptr;

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))
#define META_BLOCK_ALIGNMENT (((sizeof(struct block_meta)) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))
#define HEAP_NOT_PREALLOCATED (!heap_prealloc)
#define PAGE_SIZE getpagesize()
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define LAST_BLOCK start_block_ptr->prev
#define FIRST_BLOCK start_block_ptr
#define MMAP_THRESHOLD (128 * 1024)

struct block_meta *alloc_meta_block(size_t size);
struct block_meta *calloc_meta_block(size_t size);
struct block_meta *find_best_block(size_t size);
void add_new_block(struct block_meta *new_block);
void split(struct block_meta *block, int new_block_size);
struct block_meta *expand_last_block(int size);
struct block_meta *last_block_on_heap(void);
void coalesce(void);

#endif
