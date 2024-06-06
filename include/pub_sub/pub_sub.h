/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PUB_SUB_PUB_SUB_H_
#define PUB_SUB_PUB_SUB_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <pub_sub/msg_alloc.h>

struct pub_sub_broker {
	struct k_work work;
	struct k_work_q *work_q;
	struct k_fifo msg_fifo;
	struct k_mutex sub_list_mutex;
	sys_slist_t subscribers;
};

// Internal use, only exposed to allow static initialization of brokers
void pub_sub_broker_work_handler(struct k_work *work);

/**
 * @brief Statically initialize a broker
 *
 * @param _self The broker that is being initialized
 * @param _work_q The work queue the broker is to run on
 */
#define PUB_SUB_BROKER_INTIALIZER(_self, _work_q)                                                  \
	{                                                                                          \
		.work = Z_WORK_INITIALIZER(pub_sub_broker_work_handler), .work_q = _work_q,        \
		.msg_fifo = Z_FIFO_INITIALIZER(_self.msg_fifo),                                    \
		.sub_list_mutex = Z_MUTEX_INITIALIZER(_self.sub_list_mutex), .subscribers = {},    \
	}

/**
 * @brief Define and initialize a broker
 *
 * @param _name The name of the defined broker
 * @param _work_q The work queue the broker is to run on
 */
#define PUB_SUB_BROKER_DEFINE(_name, _work_q)                                                      \
	struct pub_sub_broker _name = PUB_SUB_BROKER_INTIALIZER(_name, _work_q)

// Forward declaration
struct pub_sub_subscriber;

/** @brief The signature for a subscriber's message handler function.
 *
 * @param subscriber The subscriber that is receiving the message
 * @param msg_id The id of the message received by the subscriber
 * @param msg The message received by the subscriber
 */
typedef void (*pub_sub_handler_fn)(struct pub_sub_subscriber *subscriber, uint16_t msg_id,
				   const void *msg);

struct pub_sub_subscriber {
	struct k_work work;
	struct k_work_q *work_q;
	struct pub_sub_broker *broker;
	sys_snode_t sub_list_node;
	pub_sub_handler_fn msg_handler;
	atomic_t *subs_bitarray;
	struct k_fifo fifo;
	uint16_t max_pub_msg_id;
	// 0 is highest priority, 255 is lowest priority
	uint8_t priority;
};

// Internal use, only exposed to allow static initialization of subscribers
void pub_sub_subscriber_work_handler(struct k_work *work);

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
 * @param _work_q The work queue the subscriber is to run on
 * @param _handler_fn The message handler function of the subscriber
 * @param _subs_bitarray The subscriptions bit array to use to track subscriptions
 * @param _max_msg_id The maximum message id that the subscriber will subscribe to
 * @param _priority The priority value to set, 0 is highest priority, 255 is lowest priority
 */
#define PUB_SUB_SUBSCRIBER_INITIALIZER(_self, _work_q, _handler_fn, _subs_bitarray, _max_msg_id,   \
				       _priority)                                                  \
	{                                                                                          \
		.work = Z_WORK_INITIALIZER(pub_sub_subscriber_work_handler), .work_q = _work_q,    \
		.broker = NULL,                                                                    \
		.sub_list_node =                                                                   \
			{                                                                          \
				.next = NULL,                                                      \
			},                                                                         \
		.msg_handler = _handler_fn, .subs_bitarray = _subs_bitarray,                       \
		.fifo = Z_FIFO_INITIALIZER(_self.fifo), .max_pub_msg_id = _max_msg_id,             \
		.priority = _priority,                                                             \
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
 * @param _work_q The work queue the subscriber is to run on
 * @param _handler_fn The message handler function of the subscriber
 * @param _max_msg_id The maximum message id that the subscriber will subscribe to
 * @param _priority The priority value to set, 0 is highest priority, 255 is lowest priority
 */
#define PUB_SUB_SUBSCRIBER_INIT_COMPOSED(_composite, _work_q, _handler_fn, _max_msg_id, _priority) \
	._subscriber =                                                                             \
		PUB_SUB_SUBSCRIBER_INITIALIZER(_composite._subscriber, _work_q, _handler_fn,       \
					       _composite._subs_bitarray, _max_msg_id, _priority), \
	._subs_bitarray = {}

/**
 * @brief Retrieve a pointer to the composed subscriber from the composite struct
 *
 * @param _composite The struct to get the subscriber pointer from
 */
#define PUB_SUB_COMPOSED_SUBSCRIBER_PTR(_composite) (&((_composite)->_subscriber))

/**
 * @brief Retrieve a pointer to the composite struct from a composed subscriber pointer
 *
 * @param _ptr A pointer to the subscriber
 * @param _type The name of the type of the composite struct
 */
#define PUB_SUB_CONTAINER_FROM_SUBSCRIBER(_ptr, _type) CONTAINER_OF(_ptr, _type, _subscriber)

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
 * @brief Add a subscriber to a  broker
 *
 * A subscriber must be added to a broker to receive any published messages.
 *
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
	pub_sub_msg_fifo_put(&broker->msg_fifo, msg);
	k_work_submit_to_queue(broker->work_q, &broker->work);
}

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
 * 0 is the highest priority value and 255 is the lowest priority value.
 *
 * @param subscriber Address of the subscriber
 * @param work_q The work_q the subscriber is to run on
 * @param msg_handler The message handler function of the subscriber
 * @param subs_bitarray The subscriptions bit array to use to track
 * subscriptions
 * @param max_pub_msg_id The maximum message id that will be subscribed to
 * @param priority The priority value to set, 0 is highest priority, 255 is lowest priority
 */
void pub_sub_init_subscriber(struct pub_sub_subscriber *subscriber, struct k_work_q *work_q,
			     pub_sub_handler_fn msg_handler, atomic_t *subs_bitarray,
			     uint16_t max_pub_msg_id, uint8_t priority);

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
static inline void pub_sub_publish_to_subscriber(struct pub_sub_subscriber *subscriber,
						 const void *msg)
{
	__ASSERT(subscriber != NULL, "");
	__ASSERT(pub_sub_msg_get_msg_id(msg) > subscriber->max_pub_msg_id,
		 "Public messages can not be published directly to subscriber");
	pub_sub_msg_fifo_put(&subscriber->fifo, msg);
	k_work_submit_to_queue(subscriber->work_q, &subscriber->work);
}

#ifdef CONFIG_PUB_SUB_DEFAULT_BROKER

extern struct pub_sub_broker g_pub_sub_default_broker;

/**
 * @brief Add a subscriber to the default broker
 *
 * A subscriber must be added to a broker to receive any published messages.
 *r.
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

#endif /* PUB_SUB_PUB_SUB_H_ */