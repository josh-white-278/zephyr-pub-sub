/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PUB_SUB_MSG_ALLOC_H_
#define PUB_SUB_MSG_ALLOC_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <pub_sub/msg.h>
#include <stddef.h>
#include <stdint.h>
#include <zephyr/kernel.h>

// Special allocator IDs
#define PUB_SUB_ALLOC_ID_STATIC_MSG   0xFF
#define PUB_SUB_ALLOC_ID_CALLBACK_MSG 0xFE

typedef void *(*pub_sub_alloc_fn)(void *impl, size_t msg_size_bytes, k_timeout_t timeout);
typedef void (*pub_sub_free_fn)(void *impl, const void *msg);

struct pub_sub_allocator {
	pub_sub_alloc_fn allocate;
	pub_sub_free_fn free;
	size_t max_msg_size;
	void *impl;
};

struct pub_sub_allocators {
	struct pub_sub_allocator *allocators[CONFIG_PUB_SUB_ALLOC_MAX_NUM];
	size_t num_allocators;
};

/**
 * @brief Initialize a collection of allocators
 *
 * A collection of allocators must be initialized prior to individual allocators being added to the
 * collection
 *
 * @param allocators Address of the collection of allocators
 */
void pub_sub_alloc_init_allocators(struct pub_sub_allocators *allocators);

/**
 * @brief Add a message allocator to a collection of allocators
 *
 * Adds the allocator so that messages can be allocated from it. An allocator can be added to
 * multiple collections.
 *
 * @warning
 * Do not add allocators after messages have begun to be allocated from the collection. The
 * allocators are stored in a sorted list and are referenced by index in the allocated messages.
 * Therefore adding a new allocator will result in any allocated messages having an invalid
 * allocator reference and it won't be able to be freed.
 *
 * @param allocators Address of the collection of allocators
 * @param allocator Address of the allocator to add
 *
 * @retval 0 Allocator added successfully
 * @retval -ENOMEM If there was no space to store the allocator
 */
int pub_sub_alloc_add(struct pub_sub_allocators *allocators, struct pub_sub_allocator *allocator);

/**
 * @brief Allocate a new message from a collection of allocators
 *
 * The allocated message can be used until it is released back to the collection of allocators from
 * which it was allocated.
 *
 * @param allocators Address of the collection of allocators
 * @param msg_id The message id to assign to the new message
 * @param msg_size_bytes The size of the message to allocate
 * @param timeout How long to wait for a message to become free
 *
 * @retval A pointer to the allocated message
 * @retval NULL If the message allocation failed
 */
void *pub_sub_alloc_new(struct pub_sub_allocators *allocators, uint16_t msg_id,
			size_t msg_size_bytes, k_timeout_t timeout);

/**
 * @brief Release a message back to a collection of allocators
 *
 * Every allocated message must be released back to the collection of allocators for it to be
 * re-used. A message must be released for each additional reference to a message that is
 * acquired.
 *
 * @param allocators Address of the collection of allocators that the message was allocated from
 * @param msg Address of the message to release
 */
void pub_sub_alloc_release(struct pub_sub_allocators *allocators, const void *msg);

#ifdef __cplusplus
}
#endif

#endif /* PUB_SUB_MSG_ALLOC_H_ */