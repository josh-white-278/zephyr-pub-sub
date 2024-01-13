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
#define PUB_SUB_ALLOC_ID_INVALID      0xFF
#define PUB_SUB_ALLOC_ID_STATIC_MSG   0xFE
#define PUB_SUB_ALLOC_ID_CALLBACK_MSG 0xFD

typedef void *(*pub_sub_alloc_fn)(void *impl, size_t msg_size_bytes, k_timeout_t timeout);
typedef void (*pub_sub_free_fn)(void *impl, const void *msg);

struct pub_sub_allocator {
	pub_sub_alloc_fn allocate;
	pub_sub_free_fn free;
	void *impl;
	uint8_t allocator_id;
};

/**
 * @brief Add a message allocator during run time
 *
 * @warning
 * A message can not be allocated from an allocator until it has been added. This is due to how
 * messages track which allocator they belong to. Without being added the allocator will not have
 * a valid allocator id so can not be tracked correctly.
 *
 * @param allocator Address of the allocator to add
 *
 * @retval 0 Allocator added successfully
 * @retval -ENOMEM If there was no space to store the allocator
 */
int pub_sub_add_runtime_allocator(struct pub_sub_allocator *allocator);

/**
 * @brief Allocate a new message from an allocator
 *
 * Allocating a message acquires a reference to it. The message can then be used until the reference
 * is released or ownership of the reference is transferred e.g. by publishing the message.
 *
 * @param allocator Address of the allocator
 * @param msg_id The message id to assign to the new message
 * @param msg_size_bytes The size of the message to allocate
 * @param timeout How long to wait for a message to become free
 *
 * @retval A pointer to the allocated message
 * @retval NULL If the message allocation failed
 */
static inline void *pub_sub_new_msg(struct pub_sub_allocator *allocator, uint16_t msg_id,
				    size_t msg_size_bytes, k_timeout_t timeout)
{
	__ASSERT(allocator != NULL, "");
	__ASSERT(allocator->allocator_id != PUB_SUB_ALLOC_ID_INVALID, "");
	void *msg = allocator->allocate(allocator->impl, msg_size_bytes, timeout);
	if (msg != NULL) {
		pub_sub_msg_init(msg, msg_id, allocator->allocator_id);
	}
	return msg;
}

/**
 * @brief Acquire a reference to a message
 *
 * Every reference that is acquired must be released before the message will be freed.
 *
 * @param msg Address of the message to acquire
 */
static inline void pub_sub_acquire_msg(const void *msg)
{
	__ASSERT(msg != NULL, "");
	pub_sub_msg_inc_ref_cnt(msg);
}

/**
 * @brief Release a reference to a message
 *
 * Every acquired reference to a message must be released before it can be re-used. If a reference
 * is ever dropped without being released then the message will leak.
 *
 * @param msg Address of the message to release
 */
void pub_sub_release_msg(const void *msg);

#ifdef __cplusplus
}
#endif

#endif /* PUB_SUB_MSG_ALLOC_H_ */