/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PUB_SUB_SUBSCRIBER_H_
#define PUB_SUB_SUBSCRIBER_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <zephyr/kernel.h>

struct pub_sub_subscriber;

typedef void (*pub_sub_handler_fn)(struct pub_sub_subscriber *subscriber, uint16_t msg_id,
				   const void *msg);

enum pub_sub_rx_type {
	PUB_SUB_RX_TYPE_CALLBACK,
	PUB_SUB_RX_TYPE_FIFO,
};

struct pub_sub_subscriber {
	struct pub_sub_broker *broker;
	sys_snode_t sub_list_node;
	pub_sub_handler_fn msg_handler;
	atomic_t *subs_bitarray;
	struct k_fifo fifo;
	uint16_t max_pub_msg_id;
	enum pub_sub_rx_type rx_type;
	// Priority is relative to other subscribers of the same type i.e. a low priority callback
	// will always be higher priority than a high priority fifo.
	// 0 is highest priority, 255 is lowest priority
	uint8_t priority;
};

#define PUB_SUB_SUBS_BITARRAY_BYTE_LEN(_max_msg_id)                                                \
	(ATOMIC_BITMAP_SIZE(_max_msg_id + 1) * sizeof(atomic_t))

/**
 * @brief Define a bitarray for a subscriber
 *
 * @param _name The name of the created bitarray
 * @param _max_msg_id The maximum message id that will be published
 */
#define PUB_SUB_SUBS_BITARRAY_DEFINE(_name, _max_msg_id) ATOMIC_DEFINE(_name, _max_msg_id + 1)

/**
 * @brief Statically initialize a subscriber
 *
 * @param _self The subscriber that is being initialized
 * @param _handler_fn The message handler function of the subscriber
 * @param _subs_bitarray The subscriptions bit array to use to track subscriptions
 * @param _max_msg_id The maximum message id that the subscriber will subscribe to
 * @param _rx_type The type of subscriber
 * @param _priority The priority value to set
 */
#define PUB_SUB_SUBSCRIBER_INITIALIZER(_self, _handler_fn, _subs_bitarray, _max_msg_id, _rx_type,  \
				       _priority)                                                  \
	{                                                                                          \
		.broker = NULL,                                                                    \
		.sub_list_node =                                                                   \
			{                                                                          \
				.next = NULL,                                                      \
			},                                                                         \
		.msg_handler = _handler_fn, .subs_bitarray = _subs_bitarray,                       \
		.fifo = Z_FIFO_INITIALIZER(_self.fifo), .max_pub_msg_id = _max_msg_id,             \
		.rx_type = _rx_type, .priority = _priority,                                        \
	}

/**
 * @brief Compose a subscriber into another struct
 *
 * @param _max_msg_id The maximum message id that the subscriber will subscribe to
 */
#define PUB_SUB_SUBSCRIBER_COMPOSE(_max_msg_id)                                                    \
	struct pub_sub_subscriber _subscriber;                                                     \
	PUB_SUB_SUBS_BITARRAY_DEFINE(_subs_bitarray, _max_msg_id)

/**
 * @brief Statically initialize a subscriber that was composed into another struct with
 * PUB_SUB_SUBSCRIBER_COMPOSE
 *
 * @param _composite The composite struct the subscriber belongs to
 * @param _handler_fn The message handler function of the subscriber
 * @param _max_msg_id The maximum message id that the subscriber will subscribe to
 * @param _rx_type The type of subscriber
 * @param _priority The priority value to set
 */
#define PUB_SUB_SUBSCRIBER_INIT_COMPOSED(_composite, _handler_fn, _max_msg_id, _rx_type,           \
					 _priority)                                                \
	._subscriber = PUB_SUB_SUBSCRIBER_INITIALIZER(_composite._subscriber, _handler_fn,         \
						      _composite._subs_bitarray, _max_msg_id,      \
						      _rx_type, _priority),                        \
	._subs_bitarray = {}

/**
 * @brief Retrieve a pointer to the composed subscriber from the composite struct
 *
 * @param _composite The struct to get the subscriber pointer from
 */
#define PUB_SUB_COMPOSED_SUBSCRIBER_PTR(_composite) (&(_composite)._subscriber)

/**
 * @brief Retrieve a pointer to the composite struct from a composed subscriber pointer
 *
 * @param _ptr A pointer to the subscriber
 * @param _type The name of the type of the composite struct
 */
#define PUB_SUB_CONTAINER_FROM_SUBSCRIBER(_ptr, _type) CONTAINER_OF(_ptr, _type, _subscriber)

/**
 * @brief Initialize a subscriber
 *
 * The subscriptions bit array must be sized correctly for the maximum
 * message id that will be subscribed to. The PUB_SUB_SUBS_BITARRAY_*
 * macros can be used to assist with creating a subscriptions bit array of
 * the correct length.
 *
 * The handler function is called by the publish subscribe framework with
 * any published messages that the subscriber has subscribed to. Messages
 * received in the handler function are read only and should not be
 * modified. Additionally, ownership of a reference to the message is not
 * passed into the handler function so if the message needs to be retained
 * past the scope of the handler function an additional reference must be
 * acquired.
 *
 * A subscriber's priority value is relative to subscribers of the same
 * type e.g. callback and fifo. 0 is the highest priority value and 255 is
 * the lowest priority value.
 *
 * @param subscriber Address of the subscriber
 * @param msg_handler The message handler function of the subscriber
 * @param subs_bitarray The subscriptions bit array to use to track
 * subscriptions
 * @param max_pub_msg_id The maximum message id that will be subscribed to
 * @param rx_type The type of subscriber
 * @param priority The priority value to set
 */
void pub_sub_init_subscriber(struct pub_sub_subscriber *subscriber, pub_sub_handler_fn msg_handler,
			     atomic_t *subs_bitarray, uint16_t max_pub_msg_id,
			     enum pub_sub_rx_type rx_type, uint8_t priority);

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
 * There is a chance that a subscriber could still receive a message after
 * unsubscribing from it if the message is already in the subscriber's
 * fifo
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
 * @brief Handle a message for a subscriber
 *
 * Dequeues a message from the subscriber's internal fifo and then calls
 * the subscriber's message handler function with the dequeued message.
 *
 * @param subscriber Address of the subscriber
 * @param timeout How long to wait for a message
 *
 * @retval 0 if handled successfully
 * @retval -ENOMSG If there was no message to handle within the specified
 * timeout
 * @retval -EPERM If the subscriber is a callback type
 */
int pub_sub_handle_queued_msg(struct pub_sub_subscriber *subscriber, k_timeout_t timeout);

/**
 * @brief Populate a k_poll_event from a subscriber
 *
 * Allows a subscriber's internal fifo to be polled for new messages
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
 * Only private messages (message id greater than the subscriber's max
 * public id) can be published directly to a subscriber. It bypasses the
 * subscriber's subscription list and is always received.
 *
 * Publishing a message passes ownership of the message's reference to the
 * subscriber i.e. after publish is called the memory pointed to by 'msg'
 * should not be accessed again. A message can only be published to a
 * single subscriber even if multiple references are owned.
 *
 * @param subscriber Address of the subscriber to publish to
 * @param msg Address of the message to publish
 */
void pub_sub_publish_to_subscriber(struct pub_sub_subscriber *subscriber, const void *msg);

#ifdef __cplusplus
}
#endif

#endif /* PUB_SUB_SUBSCRIBER_H_ */