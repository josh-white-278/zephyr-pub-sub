/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PUB_SUB_BROKER_H_
#define PUB_SUB_BROKER_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <zephyr/kernel.h>
#include <pub_sub/subscriber.h>
#include <pub_sub/msg_alloc.h>

struct pub_sub_broker {
	struct pub_sub_allocators allocators;
	struct k_fifo msg_publish_fifo;
	struct k_mutex sub_list_mutex;
	sys_slist_t subscribers;
	struct k_work_poll publish_work;
	struct k_poll_event publish_work_poll_event;
};

/**
 * @brief Initialize a broker
 *
 * A broker must be initialized before it can be used
 *
 * @param broker Address of the broker to initialize
 */
void pub_sub_init_broker(struct pub_sub_broker *broker);

/**
 * @brief Add a message allocator to a broker
 *
 * Adds the allocator so that messages can be allocated from it. An allocator can be added to
 * multiple brokers.
 *
 * @warning
 * Do not add allocators after messages have begun to be allocated from the broker. The allocators
 * are stored in a sorted list and are referenced by index in the allocated messages. Therefore
 * adding a new allocator will result in any allocated messages having an invalid allocator
 * reference and it won't be able to be freed.
 *
 * @param broker Address of the broker to add the allocator to
 * @param allocator Address of the allocator to add to the broker
 *
 * @retval 0 Allocator added successfully
 * @retval -ENOMEM If there is no space in the broker to store the allocator
 */
static inline int pub_sub_add_allocator_to_broker(struct pub_sub_broker *broker,
						  struct pub_sub_allocator *allocator)
{
	__ASSERT(broker != NULL, "");
	__ASSERT(allocator != NULL, "");
	return pub_sub_alloc_add(&broker->allocators, allocator);
}

/**
 * @brief Add a subscriber to a  broker
 *
 * A subscriber must be added to a broker to receive any published messages.
 *
 * @warning
 * A subscriber must have its message handler function set before it is added to a broker.
 * @warning
 * Subscribers can only be added to a single broker. If a subscriber needs to switch to a different
 * broker it must first be removed from its current broker before being added to the new one.
 *
 * @param broker Address of the broker to add the subscriber to
 * @param subscriber Address of the subscriber to add to the broker
 */
void pub_sub_add_subscriber_to_broker(struct pub_sub_broker *broker,
				      struct pub_sub_subscriber *subscriber);

/**
 * @brief Remove a subscriber from its broker
 *
 * Removing a subscriber from a broker will stop it receiving any published messages.
 *
 * @param subscriber Address of the subscriber to remove the broker from
 */
void pub_sub_subscriber_remove_broker(struct pub_sub_subscriber *subscriber);

/**
 * @brief Allocate a new message from a broker
 *
 * The allocated message must either be published or released making sure to use the broker it was
 * allocated from in either case.
 *
 * @param broker Address of the broker to allocate the message from
 * @param msg_id The message id to assign to the new message
 * @param msg_size_bytes The size of the message to allocate
 * @param timeout How long to wait for a message to become free
 *
 * @retval A pointer to the allocated message
 * @retval NULL If the message allocation failed
 */
static inline void *pub_sub_new_msg_from_broker(struct pub_sub_broker *broker, uint16_t msg_id,
						size_t msg_size_bytes, k_timeout_t timeout)
{
	__ASSERT(broker != NULL, "");
	return pub_sub_alloc_new(&broker->allocators, msg_id, msg_size_bytes, timeout);
}

/**
 * @brief Publish a message to a broker
 *
 * The message must have been allocated from the broker or it can be a static message defined in
 * pub_sub/static_msg.h. If it is an allocated message then ownership of the message passes to the
 * broker i.e. after publish is called the memory pointed to by 'msg' should not be accessed again.
 * If msg is a static message then its memory should not be accessed after publish until either its
 * reference counter has become 0 in the case of a static_msg or its callback has been called in the
 * case of a callback_msg.
 *
 * @param broker Address of the broker to publish to
 * @param msg Address of the message to publish
 */
static inline void pub_sub_publish_to_broker(struct pub_sub_broker *broker, void *msg)
{
	__ASSERT(broker != NULL, "");
	__ASSERT(msg != NULL, "");
	pub_sub_msg_fifo_put(&broker->msg_publish_fifo, msg);
}

/**
 * @brief Release a message back to a broker
 *
 * Every allocated message must be released back to the broker if it is not published. Also a
 * message must be released for each additional reference to a message that is acquired.
 *
 * @param broker Address of the broker that the message was allocated from
 * @param msg Address of the message to release
 */
static inline void pub_sub_msg_release_with_broker(struct pub_sub_broker *broker, const void *msg)
{
	__ASSERT(broker != NULL, "");
	__ASSERT(msg != NULL, "");
	pub_sub_alloc_release(&broker->allocators, msg);
}

#ifdef __cplusplus
}
#endif

#endif /* PUB_SUB_BROKER_H_ */