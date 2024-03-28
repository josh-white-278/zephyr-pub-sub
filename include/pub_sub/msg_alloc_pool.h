/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PUB_SUB_MSG_ALLOC_POOL_H_
#define PUB_SUB_MSG_ALLOC_POOL_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <pub_sub/msg_alloc.h>

struct pub_sub_alloc_pool {
	struct pub_sub_allocator **allocators;
	size_t *max_msg_sizes;
	size_t max_len;
	size_t len;
};

/**
 * @brief Statically define and initialize a message allocator pool
 *
 * @param name Name of the allocator
 * @param num_allocators The number of allocators in the pool
 */
#define PUB_SUB_ALLOC_POOL_STATIC_DEFINE(name, num_allocators)                                     \
	static struct pub_sub_allocator *_pub_sub_alloc_pool_allocators_##name[num_allocators] =   \
		{};                                                                                \
	static size_t _pub_sub_alloc_pool_max_msg_sizes_##name[num_allocators] = {};               \
	static struct pub_sub_alloc_pool name = {                                                  \
		.allocators = _pub_sub_alloc_pool_allocators_##name,                               \
		.max_msg_sizes = _pub_sub_alloc_pool_max_msg_sizes_##name,                         \
		.max_len = num_allocators,                                                         \
		.len = 0,                                                                          \
	}

/**
 * @brief Add a message allocator to an allocator pool
 *
 * @warning
 * Adding an allocator is not thread safe, ideally all of the allocators should be added to the pool
 * before messages are allocated from it.
 *
 * @param alloc_pool Address of the allocator pool
 * @param allocator Address of the allocator to add
 * @param allocator_max_msg_size The maximum message size that can be allocated from the allocator
 *
 * @retval 0 Allocator added successfully
 * @retval -ENOMEM If there was no space to store the allocator
 */
int pub_sub_alloc_pool_add_alloc(struct pub_sub_alloc_pool *alloc_pool,
				 struct pub_sub_allocator *allocator,
				 size_t allocator_max_msg_size);

/**
 * @brief Allocate a new message from an allocator pool
 *
 * Allocating a message acquires a reference to it. The message can then be used until the reference
 * is released or ownership of the reference is transferred e.g. by publishing the message.
 *
 * @param alloc_pool Address of the allocator pool
 * @param msg_id The message id to assign to the new message
 * @param msg_size_bytes The size of the message to allocate
 * @param timeout How long to wait for a message to become free
 *
 * @retval A pointer to the allocated message
 * @retval NULL If the message allocation failed
 */
void *pub_sub_alloc_pool_new(struct pub_sub_alloc_pool *alloc_pool, uint16_t msg_id,
			     size_t msg_size_bytes, k_timeout_t timeout);

#ifdef __cplusplus
}
#endif

#endif /* PUB_SUB_MSG_ALLOC_POOL_H_ */