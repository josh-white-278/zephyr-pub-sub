/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PUB_SUB_RUNTIME_SUBSCRIBER_H_
#define PUB_SUB_RUNTIME_SUBSCRIBER_H_
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif
#if defined(CONFIG_PUB_SUB_RUNTIME_SUBSCRIBERS)

// Forward declarations
struct pub_sub_broker;
struct pub_sub_subscriber;
struct pub_sub_broker_subscriber_entry;
typedef void (*pub_sub_handler_fn)(struct pub_sub_subscriber *subscriber, uint16_t msg_id,
				   const void *msg);

/**
 * @brief Initialize a broker
 *
 * A broker must be initialized before it can be used
 *
 * @param broker Address of the broker to initialize
 * @param broker Address of the work queue the broker is to run on
 */
void pub_sub_init_broker(struct pub_sub_broker *broker, struct k_work_q *work_q);

/**
 * @brief Initialize a subscriber
 *
 * The subscriptions bit array must be sized correctly for the maximum message id that will be
 * subscribed to. The PUB_SUB_SUBS_BITARRAY_* macros can be used to assist with creating a
 * subscriptions bit array of the correct length.
 *
 * The handler function is called by the publish subscribe framework with any published messages
 * that the subscriber has subscribed to. Messages received in the handler function are read only
 * and should not be modified. Additionally, ownership of a reference to the message is not passed
 * into the handler function so if the message needs to be retained past the scope of the handler
 * function an additional reference must be acquired.
 *
 * @param subscriber Address of the subscriber
 * @param work_q The work_q the subscriber is to run on
 * @param msg_handler The message handler function of the subscriber
 * @param subs_bitarray The subscriptions bit array to use to track subscriptions
 * @param max_pub_msg_id The maximum message id that will be subscribed to
 */
void pub_sub_init_subscriber(struct pub_sub_subscriber *subscriber, struct k_work_q *work_q,
			     pub_sub_handler_fn msg_handler, atomic_t *subs_bitarray,
			     uint16_t max_pub_msg_id);

/**
 * @brief Add a subscriber to a broker
 *
 * A subscriber must be added to a broker to receive any published messages.
 *
 * @warning
 * Subscribers can only be added to a single broker. If a subscriber needs to switch to a different
 * broker it must first be removed from its current broker before being added to the new one.
 *
 * @param broker Address of the broker to add the subscriber to
 * @param subscriber Address of the subscriber to add to the broker
 * @param priority The priority value to set, 0 is highest priority, 99 is lowest priority
 */
void pub_sub_add_subscriber_to_broker(struct pub_sub_broker *broker,
				      struct pub_sub_subscriber *subscriber, uint8_t priority);

/**
 * @brief Remove a subscriber from its broker
 *
 * Removing a subscriber from a broker will stop it receiving any published messages.
 *
 * @param subscriber Address of the subscriber to remove the broker from
 */
void pub_sub_subscriber_remove_broker(struct pub_sub_subscriber *subscriber);

#ifdef CONFIG_PUB_SUB_DEFAULT_BROKER

extern struct pub_sub_broker g_pub_sub_default_broker;

/**
 * @brief Add a subscriber to the default broker
 *
 * A subscriber must be added to a broker to receive any published messages.
 *
 * @warning
 * Subscribers can only be added to a single broker. If a subscriber needs to switch to a different
 * broker it must first be removed from its current broker before being added to the new one.
 *
 * @param subscriber Address of the subscriber to add
 * @param priority The priority value to set, 0 is highest priority, 99 is lowest priority
 */
static inline void pub_sub_add_subscriber(struct pub_sub_subscriber *subscriber, uint8_t priority)
{
	pub_sub_add_subscriber_to_broker(&g_pub_sub_default_broker, subscriber, priority);
}

#endif // CONFIG_PUB_SUB_DEFAULT_BROKER

#endif // CONFIG_PUB_SUB_RUNTIME_SUBSCRIBERS
#ifdef __cplusplus
}
#endif

#endif /* PUB_SUB_RUNTIME_SUBSCRIBER_H_ */