/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <pub_sub/msg_alloc_mem_slab.h>

void pub_sub_init_mem_slab_allocator(struct pub_sub_allocator *allocator,
				     struct k_mem_slab *mem_slab)
{
	__ASSERT(allocator != NULL, "");
	__ASSERT(mem_slab != NULL, "");
	allocator->allocate = pub_sub_allocate_from_mem_slab;
	allocator->free = pub_sub_free_for_mem_slab;
	allocator->allocator_id = PUB_SUB_ALLOC_ID_INVALID;
	allocator->impl = mem_slab;
}

void *pub_sub_allocate_from_mem_slab(void *impl, size_t msg_size_bytes, k_timeout_t timeout)
{
	__ASSERT(impl != NULL, "");
	struct k_mem_slab *mem_slab = impl;
	__ASSERT(msg_size_bytes <= mem_slab->info.block_size - PUB_SUB_MSG_OVERHEAD_NUM_BYTES, "");
	struct pub_sub_msg *ps_msg = NULL;
	int res = k_mem_slab_alloc(mem_slab, (void **)&ps_msg, timeout);
	return res == 0 ? ps_msg->msg : NULL;
}

void pub_sub_free_for_mem_slab(void *impl, const void *msg)
{
	__ASSERT(impl != NULL, "");
	__ASSERT(msg != NULL, "");
	struct k_mem_slab *mem_slab = impl;
	struct pub_sub_msg *ps_msg = CONTAINER_OF(msg, struct pub_sub_msg, msg);
	k_mem_slab_free(mem_slab, ps_msg);
}