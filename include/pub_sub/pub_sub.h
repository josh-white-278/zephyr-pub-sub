/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PUB_SUB_PUB_SUB_H_
#define PUB_SUB_PUB_SUB_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <pub_sub/broker.h>
#include <pub_sub/subscriber.h>
#include <pub_sub/msg_alloc.h>

/**
 * @brief Handle a message for a subscriber
 *
 * Dequeues a message from the subscriber's internal message queue or fifo and then calls the
 * subscriber's message handler function with the dequeued message.
 *
 * @param subscriber Address of the subscriber
 * @param timeout How long to wait for a message
 *
 * @retval 0 if handled successfully
 * @retval -ENOMSG If there was no message to handle within the specified timeout
 * @retval -EPERM If the subscriber is a callback type
 */
int pub_sub_handle_queued_msg(struct pub_sub_subscriber *subscriber, k_timeout_t timeout);

/**
 * @brief Populate a k_poll_event from a subscriber
 *
 * Allows a subscriber's internal message queue or fifo to be polled for new messages
 *
 * @param subscriber Address of the subscriber
 * @param poll_evt Address of poll event to populate
 *
 * @retval 0 Poll event populated successfully
 * @retval -EPERM If the subscriber is a callback type
 */
int pub_sub_populate_poll_evt(struct pub_sub_subscriber *subscriber, struct k_poll_event *poll_evt);

/**
 * @brief Allocate a new message from a subscriber's broker
 *
 * The allocated message must either be published or released making sure to use the broker it was
 * allocated from in either case.
 *
 * @param subscriber Address of the subscriber
 * @param msg_id The message id to assign to the new message
 * @param msg_size_bytes The size of the message to allocate
 * @param timeout How long to wait for a message to become free
 *
 * @retval A pointer to the allocated message
 * @retval NULL If the message allocation failed
 */
static inline void *pub_sub_new_msg_from_sub(struct pub_sub_subscriber *subscriber, uint16_t msg_id,
					     size_t msg_size_bytes, k_timeout_t timeout)
{
	__ASSERT(subscriber != NULL, "");
	__ASSERT(subscriber->broker != NULL, "");
	return pub_sub_new_msg_from_broker(subscriber->broker, msg_id, msg_size_bytes, timeout);
}

/**
 * @brief Acquire a reference to a message
 *
 * Every reference that is acquired must be released before the message will be freed.
 *
 * @param msg Address of the message to acquire
 */
static inline void pub_sub_acquire(const void *msg)
{
	__ASSERT(msg != NULL, "");
	pub_sub_msg_inc_ref_cnt(msg);
}

/**
 * @brief Release a message back to a subscriber's broker
 *
 * Every allocated message must be released back to the broker if it is not published. Also a
 * message must be released for each additional reference to a message that is acquired.
 *
 * @param subscriber Address of the subscriber
 * @param msg Address of the message to release
 */
static inline void pub_sub_msg_release_with_sub(struct pub_sub_subscriber *subscriber,
						const void *msg)
{
	__ASSERT(subscriber != NULL, "");
	__ASSERT(subscriber->broker != NULL, "");
	__ASSERT(msg != NULL, "");
	pub_sub_msg_release_with_broker(subscriber->broker, msg);
}

#ifdef CONFIG_PUB_SUB_DEFAULT_BROKER

extern struct pub_sub_broker g_pub_sub_default_broker;

/**
 * @brief Add a message allocator to the default broker
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
 * @param allocator Address of the allocator to add to the broker
 *
 * @retval 0 Allocator added successfully
 * @retval -ENOMEM If there is no space in the broker to store the allocator
 */
static inline int pub_sub_add_allocator(struct pub_sub_allocator *allocator)
{
	return pub_sub_add_allocator_to_broker(&g_pub_sub_default_broker, allocator);
}

/**
 * @brief Add a subscriber to the default broker
 *
 * A subscriber must be added to a broker to receive any published messages.
 *
 * @warning
 * A subscriber must have its message handler function set before it is added to a broker.
 * @warning
 * Subscribers can only be added to a single broker. If a subscriber needs to switch to a different
 * broker it must first be removed from its current broker before being added to the new one.
 *
 * @param subscriber Address of the subscriber to add
 */
static inline void pub_sub_add_subscriber(struct pub_sub_subscriber *subscriber)
{
	pub_sub_add_subscriber_to_broker(&g_pub_sub_default_broker, subscriber);
}

/**
 * @brief Allocate a new message from the default broker
 *
 * The allocated message must either be published or released after allocation
 *
 * @param msg_id The message id to assign to the new message
 * @param msg_size_bytes The size of the message to allocate
 * @param timeout How long to wait for a message to become free
 *
 * @retval A pointer to the allocated message
 * @retval NULL If the message allocation failed
 */
static inline void *pub_sub_new_msg(uint16_t msg_id, size_t msg_size_bytes, k_timeout_t timeout)
{
	return pub_sub_new_msg_from_broker(&g_pub_sub_default_broker, msg_id, msg_size_bytes,
					   timeout);
}

/**
 * @brief Publish a message to the default broker
 *
 * The message must have been allocated from the default broker or it can be a static message
 * defined in pub_sub/static_msg.h. If it is an allocated message then ownership of the message
 * passes to the broker i.e. after publish is called the memory pointed to by 'msg' should not be
 * accessed again. If msg is a static message then its memory should not be accessed after publish
 * until either its reference counter has become 0 in the case of a static_msg or its callback has
 * been called in the case of a callback_msg.
 *
 * @param msg Address of the message to publish
 */
static inline void pub_sub_publish(void *msg)
{
	pub_sub_publish_to_broker(&g_pub_sub_default_broker, msg);
}

/**
 * @brief Release a message back to the default broker
 *
 * Every allocated message must be released back to the broker if it is not published. Also a
 * message must be released for each additional reference to a message that is acquired.
 *
 * @param msg Address of the message to release
 */
static inline void pub_sub_release(const void *msg)
{
	pub_sub_msg_release_with_broker(&g_pub_sub_default_broker, msg);
}

#endif // CONFIG_PUB_SUB_DEFAULT_BROKER

#ifdef __cplusplus
}
#endif

#endif /* PUB_SUB_PUB_SUB_H_ */