/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include "helpers.h"
#include <stdlib.h>
#include <pub_sub/msg_alloc_mem_slab.h>
#include <zephyr/ztest.h>

struct pub_sub_allocator *malloc_mem_slab_allocator(size_t msg_size, size_t num_msgs)
{
	uint8_t *buffer = malloc(PUB_SUB_MEM_SLAB_ALLOCATOR_BUF_SIZE(msg_size, num_msgs));
	struct k_mem_slab *mem_slab = malloc(sizeof(struct k_mem_slab));
	struct pub_sub_allocator *allocator = malloc(sizeof(struct pub_sub_allocator));
	k_mem_slab_init(mem_slab, buffer, PUB_SUB_MEM_SLAB_ALLOCATOR_BLOCK_SIZE(msg_size),
			num_msgs);
	pub_sub_init_mem_slab_allocator(allocator, mem_slab);
	return allocator;
}

void reset_mem_slab_allocator(struct pub_sub_allocator *allocator)
{
	struct k_mem_slab *mem_slab = allocator->impl;
	k_mem_slab_init(mem_slab, mem_slab->buffer, mem_slab->info.block_size,
			mem_slab->info.num_blocks);
}

void free_mem_slab_allocator(struct pub_sub_allocator *allocator)
{
	struct k_mem_slab *mem_slab = allocator->impl;
	// Check all of mem_slab blocks are free to see if we have any leaks
	__ASSERT(k_mem_slab_num_used_get(mem_slab) == 0, "");
	free(mem_slab->buffer);
	free(mem_slab);
	free(allocator);
}
