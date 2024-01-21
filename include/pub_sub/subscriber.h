/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PUB_SUB_SUBSCRIBER_H_
#define PUB_SUB_SUBSCRIBER_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <zephyr/kernel.h>

typedef void (*pub_sub_handler_fn)(uint16_t msg_id, const void *msg, void *user_data);

enum pub_sub_rx_type {
	PUB_SUB_RX_TYPE_CALLBACK,
	PUB_SUB_RX_TYPE_MSGQ,
	PUB_SUB_RX_TYPE_FIFO,
};

struct pub_sub_subscriber_handler_data {
	pub_sub_handler_fn msg_handler;
	void *user_data;
};

struct pub_sub_subscriber {
	struct pub_sub_broker *broker;
	sys_snode_t sub_list_node;
	struct pub_sub_subscriber_handler_data handler_data;
	union {
		struct k_msgq *msgq;
		struct k_fifo fifo;
	};
	atomic_t *subs_bitarray;
	enum pub_sub_rx_type rx_type;
	uint16_t max_pub_msg_id;
	// Priority is relative to other subscribers of the same type i.e. a low priority callback
	// will always be higher priority than a high priority msgq.
	// 0 is highest priority, 255 is lowest priority
	uint8_t priority;
};

#define PUB_SUB_SUBS_BITARRAY_BYTE_LEN(max_msg_id)                                                 \
	(ATOMIC_BITMAP_SIZE(max_msg_id + 1) * sizeof(atomic_t))

/**
 * @brief Define a bitarray for a subscriber
 *
 * @param name The name of the created bitarray
 * @param max_msg_id The maximum message id that will be published
 */
#define PUB_SUB_SUBS_BITARRAY_DEFINE(name, max_msg_id) ATOMIC_DEFINE(name, max_msg_id)

#define PUB_SUB_RX_MSGQ_MSG_SIZE             (sizeof(void *))
#define PUB_SUB_RX_MSGQ_BUFFER_LEN(max_msgs) (max_msgs * PUB_SUB_RX_MSGQ_MSG_SIZE)

/**
 * @brief Statically define and initialize a message queue for a subscriber
 *
 * @param name The name of the created message queue
 * @param max_msgs The maximum number of messages that can be queued
 */
#define PUB_SUB_RX_MSGQ_DEFINE(name, max_msgs)                                                     \
	K_MSGQ_DEFINE(name, PUB_SUB_RX_MSGQ_MSG_SIZE, max_msgs, sizeof(void *));

/**
 * @brief Initialize a callback type subscriber
 *
 * A subscriber must be initialized before it is used. The subscriptions bit array must be sized
 * correctly for the maximum message id that will be subscribed to. The PUB_SUB_SUBS_BITARRAY_*
 * macros can be used to assist with creating a subscriptions bit array of the correct length.
 *
 * @param subscriber Address of the subscriber
 * @param subs_bitarray The subscriptions bit array to use to track subscriptions
 * @param max_pub_msg_id The maximum message id that will be subscribed to
 */
void pub_sub_init_callback_subscriber(struct pub_sub_subscriber *subscriber,
				      atomic_t *subs_bitarray, uint16_t max_pub_msg_id);

/**
 * @brief Initialize a message queue type subscriber
 *
 * A subscriber must be initialized before it is used. The subscriptions bit array must be sized
 * correctly for the maximum message id that will be subscribed to. The PUB_SUB_SUBS_BITARRAY_*
 * macros can be used to assist with creating a subscriptions bit array of the correct length. The
 * message queue must be initialized and sized correctly for the publish subscribe framework. The
 * PUB_SUB_RX_MSGQ_* macros can be used to assist with creating a message queue for this purpose.
 *
 * @param subscriber Address of the subscriber
 * @param subs_bitarray The subscriptions bit array to use to track subscriptions
 * @param max_pub_msg_id The maximum message id that will be subscribed to
 * @param msgq The message queue to use for queuing messages
 */
void pub_sub_init_msgq_subscriber(struct pub_sub_subscriber *subscriber, atomic_t *subs_bitarray,
				  uint16_t max_pub_msg_id, struct k_msgq *msgq);

/**
 * @brief Initialize a FIFO type subscriber
 *
 * A subscriber must be initialized before it is used. The subscriptions bit array must be sized
 * correctly for the maximum message id that will be subscribed to. The PUB_SUB_SUBS_BITARRAY_*
 * macros can be used to assist with creating a subscriptions bit array of the correct length.
 *
 * @param subscriber Address of the subscriber
 * @param subs_bitarray The subscriptions bit array to use to track subscriptions
 * @param max_pub_msg_id The maximum message id that will be subscribed to
 */
void pub_sub_init_fifo_subscriber(struct pub_sub_subscriber *subscriber, atomic_t *subs_bitarray,
				  uint16_t max_pub_msg_id);

/**
 * @brief Subscribe to a message id
 *
 * A subscriber must be subscribed to a message id to receive it
 *
 * @param subscriber Address of the subscriber
 * @param msg_id The message id to subscribe to
 */
static inline void pub_sub_subscribe(struct pub_sub_subscriber *subscriber, uint16_t msg_id)
{
	__ASSERT(subscriber != NULL, "");
	__ASSERT(subscriber->subs_bitarray != NULL, "");
	__ASSERT(msg_id <= subscriber->max_pub_msg_id, "");
	atomic_set_bit(subscriber->subs_bitarray, msg_id);
}

/**
 * @brief Unsubscribe from a message id
 *
 * @warning
 * There is a chance that a subscriber could still receive a message after unsubscribing from it if
 * the message is already in the subscriber's message queue
 *
 * @param subscriber Address of the subscriber
 * @param msg_id The message id to unsubscribe from
 */
static inline void pub_sub_unsubscribe(struct pub_sub_subscriber *subscriber, uint16_t msg_id)
{
	__ASSERT(subscriber != NULL, "");
	__ASSERT(subscriber->subs_bitarray != NULL, "");
	__ASSERT(msg_id <= subscriber->max_pub_msg_id, "");
	atomic_clear_bit(subscriber->subs_bitarray, msg_id);
}

/**
 * @brief Set a subscriber's message handler function and data
 *
 * A subscriber must have a handler function set before it is added to a broker. The handler
 * function is called by the publish subscribe framework with any published messages that the
 * subscriber has subscribed to. Messages received in the handler function are read only and should
 * not be modified. Additionally, ownership of a reference to the message is not passed into the
 * handler function so if the message needs to be retained past the scope of the handler function
 * an additional reference must be acquired.
 *
 * @warning
 * After a subscriber has been added to a broker its handler data should only be changed from
 * within the handler function itself. Setting the handler data is not atomic so updating it from
 * within the handler function removes any race conditions.
 *
 * @param subscriber Address of the subscriber
 * @param priority The priority value to set
 */
static inline void pub_sub_subscriber_set_handler_data(struct pub_sub_subscriber *subscriber,
						       pub_sub_handler_fn msg_handler,
						       void *user_data)
{
	__ASSERT(subscriber != NULL, "");
	__ASSERT(msg_handler != NULL, "");
	subscriber->handler_data.msg_handler = msg_handler;
	subscriber->handler_data.user_data = user_data;
}

/**
 * @brief Set a subscriber's relative priority value
 *
 * A subscriber's priority value is relative to subscribers of the same type e.g. callback, msgq and
 * fifo. 0 is the highest priority value and 255 is the lowest priority value.
 *
 * @warning
 * Changing a subscriber's priority after it has been added to a broker has no effect as the
 * broker only checks the priority value when it is added.
 *
 * @param subscriber Address of the subscriber
 * @param priority The priority value to set
 */
static inline void pub_sub_subscriber_set_priority(struct pub_sub_subscriber *subscriber,
						   uint8_t priority)
{
	__ASSERT(subscriber != NULL, "");
	subscriber->priority = priority;
}

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
 * @brief Publish a message directly to a subscriber
 *
 * Only private messages (message id greater than the subscriber's max public id) can be published
 * directly to a subscriber. It bypasses the subscriber's subscription list and is always received.
 *
 * Publishing a message passes ownership of the message's reference to the subscriber i.e. after
 * publish is called the memory pointed to by 'msg' should not be accessed again. A message can only
 * be published to a single subscriber even if multiple references are owned.
 *
 * @param subscriber Address of the subscriber to publish to
 * @param msg Address of the message to publish
 */
void pub_sub_publish_to_subscriber(struct pub_sub_subscriber *subscriber, void *msg);

#ifdef __cplusplus
}
#endif

#endif /* PUB_SUB_SUBSCRIBER_H_ */