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
 * @brief Publish a message to a broker
 *
 * Publishing a message passes ownership of the message's reference to the broker i.e. after publish
 * is called the memory pointed to by 'msg' should not be accessed again. A message can only be
 * published to a single broker even if multiple references are owned.
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

#ifdef CONFIG_PUB_SUB_DEFAULT_BROKER

extern struct pub_sub_broker g_pub_sub_default_broker;

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
 * @brief Publish a message to the default broker
 *
 * Publishing a message passes ownership of the message's reference to the broker i.e. after publish
 * is called the memory pointed to by 'msg' should not be accessed again. A message can only be
 * published to a single broker even if multiple references are owned.
 *
 * @param msg Address of the message to publish
 */
static inline void pub_sub_publish(void *msg)
{
	pub_sub_publish_to_broker(&g_pub_sub_default_broker, msg);
}

#endif // CONFIG_PUB_SUB_DEFAULT_BROKER

#ifdef __cplusplus
}
#endif

#endif /* PUB_SUB_BROKER_H_ */