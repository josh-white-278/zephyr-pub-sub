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
#define PUB_SUB_ALLOC_ID_INVALID             0xFF
#define PUB_SUB_ALLOC_ID_STATIC_MSG          0xFE
#define PUB_SUB_ALLOC_ID_CALLBACK_MSG        0xFD
#define PUB_SUB_ALLOC_ID_LINK_SECTION        0xFC
#define PUB_SUB_ALLOC_ID_LINK_SECTION_MAX_ID 0x7F

#ifdef CONFIG_PUB_SUB_RUNTIME_ALLOCATORS
#define PUB_SUB_ALLOC_ID_RUNTIME_OFFSET 0x80
#endif // CONFIG_PUB_SUB_RUNTIME_ALLOCATORS

typedef void *(*pub_sub_alloc_fn)(void *impl, size_t msg_size_bytes, k_timeout_t timeout);
typedef void (*pub_sub_free_fn)(void *impl, const void *msg);

struct pub_sub_allocator {
	pub_sub_alloc_fn allocate;
	pub_sub_free_fn free;
	void *impl;
	uint8_t allocator_id;
};

#define PUB_SUB_ALLOCATOR_DEFINE(name, allocate_fn, free_fn, _impl)                                \
	STRUCT_SECTION_ITERABLE(pub_sub_allocator, name) = {                                       \
		.allocate = allocate_fn,                                                           \
		.free = free_fn,                                                                   \
		.impl = _impl,                                                                     \
		.allocator_id = PUB_SUB_ALLOC_ID_LINK_SECTION,                                     \
	}

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
		uint8_t allocator_id = allocator->allocator_id;
		if (allocator_id == PUB_SUB_ALLOC_ID_LINK_SECTION) {
			// Linker section allocators are in ROM and are all given the
			// PUB_SUB_ALLOC_ID_LINK_SECTION id when they are defined. Therefore we need
			// to calculate the real id from the allocator's index in the linker section
			STRUCT_SECTION_START_EXTERN(pub_sub_allocator);
			allocator_id = allocator - STRUCT_SECTION_START(pub_sub_allocator);
			__ASSERT(allocator_id <= PUB_SUB_ALLOC_ID_LINK_SECTION_MAX_ID, "");
		}
		pub_sub_msg_init(msg, msg_id, allocator_id);
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