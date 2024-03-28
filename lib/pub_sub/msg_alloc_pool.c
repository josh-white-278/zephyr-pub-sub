/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <pub_sub/msg_alloc_pool.h>

int pub_sub_alloc_pool_add_alloc(struct pub_sub_alloc_pool *alloc_pool,
				 struct pub_sub_allocator *allocator, size_t allocator_max_msg_size)
{
	__ASSERT(alloc_pool != NULL, "");
	__ASSERT(allocator != NULL, "");
	int ret = -ENOMEM;
	if (alloc_pool->len < alloc_pool->max_len) {
		// Insert into allocators array so that it is sorted by max msg size
		struct pub_sub_allocator *tmp;
		size_t tmp_size;
		for (size_t i = 0; i < alloc_pool->len; i++) {
			tmp = alloc_pool->allocators[i];
			tmp_size = alloc_pool->max_msg_sizes[i];
			if (allocator_max_msg_size < tmp_size) {
				alloc_pool->allocators[i] = allocator;
				alloc_pool->max_msg_sizes[i] = allocator_max_msg_size;
				allocator = tmp;
				allocator_max_msg_size = tmp_size;
			}
		}
		alloc_pool->allocators[alloc_pool->len] = allocator;
		alloc_pool->max_msg_sizes[alloc_pool->len] = allocator_max_msg_size;
		alloc_pool->len++;
		ret = 0;
	}
	return ret;
}

void *pub_sub_alloc_pool_new(struct pub_sub_alloc_pool *alloc_pool, uint16_t msg_id,
			     size_t msg_size_bytes, k_timeout_t timeout)
{
	__ASSERT(alloc_pool != NULL, "");
	void *msg = NULL;
	for (size_t i = 0; i < alloc_pool->len; i++) {
		if (alloc_pool->max_msg_sizes[i] >= msg_size_bytes) {
			struct pub_sub_allocator *allocator = alloc_pool->allocators[i];
			msg = pub_sub_new_msg(allocator, msg_id, msg_size_bytes, timeout);
			break;
		}
	}
	return msg;
}