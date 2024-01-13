/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PUB_SUB_MSG_ALLOC_MEM_SLAB_H_
#define PUB_SUB_MSG_ALLOC_MEM_SLAB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <pub_sub/msg_alloc.h>

#define PUB_SUB_MEM_SLAB_ALLOCATOR_BLOCK_SIZE(msg_size)                                            \
	WB_UP(msg_size + PUB_SUB_MSG_OVERHEAD_NUM_BYTES)
#define PUB_SUB_MEM_SLAB_ALLOCATOR_BUF_SIZE(msg_size, num_msgs)                                    \
	(PUB_SUB_MEM_SLAB_ALLOCATOR_BLOCK_SIZE(msg_size) * num_msgs)

/**
 * @brief Statically define and initialize a memory slab based message allocator
 *
 * @param name Name of the allocator
 * @param msg_size Size of each message
 * @param num_msgs Number of messages
 */
#define PUB_SUB_MEM_SLAB_ALLOCATOR_DEFINE_STATIC(name, msg_size, num_msgs)                         \
	K_MEM_SLAB_DEFINE_STATIC(_pub_sub_mem_slab_##name,                                         \
				 msg_size + PUB_SUB_MSG_OVERHEAD_NUM_BYTES, num_msgs,              \
				 sizeof(uintptr_t));                                               \
	static struct pub_sub_allocator name = {.allocate = pub_sub_allocate_from_mem_slab,        \
						.free = pub_sub_free_for_mem_slab,                 \
						.allocator_id = PUB_SUB_ALLOC_ID_INVALID,          \
						.impl = &_pub_sub_mem_slab_##name}

/**
 * @brief Initialize a memory slab based message allocator
 *
 * The memory slab must have already been initialized prior to calling this function. Additionally
 * the buffer underlying the memory slab should have been sized with either
 * PUB_SUB_MEM_SLAB_ALLOCATOR_BUF_SIZE or PUB_SUB_MEM_SLAB_ALLOCATOR_BLOCK_SIZE so that the
 * allocator overhead can be taken into account for each message.
 *
 * @param allocator Address of the allocator
 * @param mem_slab Address of the memory slab
 */
void pub_sub_init_mem_slab_allocator(struct pub_sub_allocator *allocator,
				     struct k_mem_slab *mem_slab);

/**
 * @brief Internal implementation, only exposed for PUB_SUB_MEM_SLAB_ALLOCATOR_DEFINE_STATIC
 */
void *pub_sub_allocate_from_mem_slab(void *impl, size_t msg_size_bytes, k_timeout_t timeout);

/**
 * @brief Internal implementation, only exposed for PUB_SUB_MEM_SLAB_ALLOCATOR_DEFINE_STATIC
 */
void pub_sub_free_for_mem_slab(void *impl, const void *msg);
#ifdef __cplusplus
}
#endif

#endif /* PUB_SUB_MSG_ALLOC_MEM_SLAB_H_ */