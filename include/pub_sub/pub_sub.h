/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PUB_SUB_PUB_SUB_H_
#define PUB_SUB_PUB_SUB_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <pub_sub/msg_alloc.h>
#include <pub_sub/runtime_subscriber.h>
#include <zephyr/sys/util_macro.h>

// Forward declaration
struct pub_sub_broker;
struct pub_sub_subscriber;
struct pub_sub_broker_subscriber_entry;

/** @brief The signature for a subscriber's message handler function.
 *
 * @param subscriber The subscriber that is receiving the message
 * @param msg_id The id of the message received by the subscriber
 * @param msg The message received by the subscriber
 */
typedef void (*pub_sub_handler_fn)(struct pub_sub_subscriber *subscriber, uint16_t msg_id,
				   const void *msg);

struct pub_sub_broker {
	struct k_work work;
	struct k_work_q *work_q;
	struct k_fifo msg_fifo;
	const struct pub_sub_broker_subscriber_entry *subscribers_start;
#if defined(CONFIG_PUB_SUB_RUNTIME_SUBSCRIBERS)
	struct k_mutex sub_list_mutex;
	sys_slist_t subscribers;
#endif // CONFIG_PUB_SUB_RUNTIME_SUBSCRIBERS
};

struct pub_sub_subscriber {
	struct k_work work;
	struct k_work_q *work_q;
	pub_sub_handler_fn msg_handler;
	atomic_t *subs_bitarray;
	struct k_fifo fifo;
#if defined(CONFIG_PUB_SUB_RUNTIME_SUBSCRIBERS)
	struct pub_sub_broker *broker;
	sys_snode_t sub_list_node;
	uint8_t priority;
#else
	const struct pub_sub_broker_subscriber_entry *broker_entry;
#endif // CONFIG_PUB_SUB_RUNTIME_SUBSCRIBERS
	uint16_t max_pub_msg_id;
};

struct pub_sub_broker_subscriber_entry {
	struct pub_sub_subscriber *subscriber;
#if defined(CONFIG_PUB_SUB_RUNTIME_SUBSCRIBERS)
	uint8_t priority;
#endif // CONFIG_PUB_SUB_RUNTIME_SUBSCRIBERS
};

// Used in combination with MACRO_MAP_CAT to insert '_'s
#define PUB_SUB_PREPEND_UNDERSCORE(name) _##name

/**
 * @brief Add an entry to the broker subscriber iterable section
 *
 * This macro creates a pub_sub_broker_subscriber_entry in the iterable section.
 *
 * @note In the build assert using IS_EQ to make sure that _priority is an integer literal
 *
 * @param _name The name to give the entry
 * @param _subscriber_ptr Address of the subscriber to add to the broker
 * @param _broker The name of the broker to add the subscriber to
 * @param _priority The priority of the subscriber, 0 is highest priority, 99 is lowest priority
 */
#define PUB_SUB_ADD_BROKER_ENTRY(_name, _subscriber_ptr, _broker, _priority)                       \
	BUILD_ASSERT(IS_EQ(_priority, _priority) && (_priority < 100) && (_priority >= 0),         \
		     "priority must be an integer literal between 0 and 99 inclusive");            \
	static const STRUCT_SECTION_ITERABLE_NAMED(                                                \
		pub_sub_broker_subscriber_entry,                                                   \
		MACRO_MAP_CAT(PUB_SUB_PREPEND_UNDERSCORE, _broker, subscriber, _priority),         \
		_name) = {                                                                         \
		.subscriber = _subscriber_ptr,                                                     \
		IF_ENABLED(CONFIG_PUB_SUB_RUNTIME_SUBSCRIBERS, (.priority = _priority, ))}

// Validates that _broker passed to PUB_SUB_SUBSCRIBER_ADD_TO_BROKER is the correct type
#ifdef __cplusplus
#define PUB_SUB_VALIDATE_BROKER(_broker)
#else
#define PUB_SUB_VALIDATE_BROKER(_broker)                                                           \
	BUILD_ASSERT(SAME_TYPE(_broker, *((struct pub_sub_broker *)0)),                            \
		     "broker must be of type 'struct pub_sub_broker'")
#endif

/**
 * @brief Statically add a subscriber to a broker
 *
 * This macro creates a pub_sub_broker_subscriber_entry for the subscriber in the selected broker's
 * iterable section. The broker must have been defined with PUB_SUB_BROKER_DEFINE.
 *
 * @param _subscriber_ptr Address of the subscriber to add to the broker
 * @param _broker The name of the broker to add the subscriber to
 * @param _priority The priority of the subscriber, 0 is highest priority, 99 is lowest priority
 */
#define PUB_SUB_SUBSCRIBER_ADD_TO_BROKER(_subscriber_ptr, _broker, _priority)                      \
	PUB_SUB_VALIDATE_BROKER(_broker);                                                          \
	PUB_SUB_ADD_BROKER_ENTRY(_CONCAT(_broker_entry_, __LINE__), _subscriber_ptr, _broker,      \
				 _priority)

// Internal use, only exposed to allow static initialization of brokers
void pub_sub_broker_work_handler(struct k_work *work);

/**
 * @brief Statically initialize a broker
 *
 * @param _self The broker that is being initialized
 * @param _work_q The work queue the broker is to run on
 * @param _subscribers_start The broker subscriber entry which marks the start of the broker's
 *                           subscribers
 */
#define PUB_SUB_BROKER_INITIALIZER(_self, _work_q, _subscribers_start)                             \
	{                                                                                          \
		.work = Z_WORK_INITIALIZER(pub_sub_broker_work_handler), .work_q = _work_q,        \
		.msg_fifo = Z_FIFO_INITIALIZER(_self.msg_fifo),                                    \
		.subscribers_start = _subscribers_start,                                           \
		IF_ENABLED(CONFIG_PUB_SUB_RUNTIME_SUBSCRIBERS,                                     \
			   (.sub_list_mutex = Z_MUTEX_INITIALIZER(_self.sub_list_mutex),           \
			    .subscribers = {}, ))                                                  \
	}

/**
 * @brief Define and initialize a broker
 *
 * @note When we are creating the broker's subscribers_start with PUB_SUB_ADD_BROKER_ENTRY we use
 * _CONCAT(_name, _broker) as the broker name. This ensures it will be at the start of the broker's
 * section in the subscriber entries iterable section. For example with a broker named xxxxx, the
 * broker's entry will have a section name _xxxxx_broker_subscriber_0_ and all of its subscribers
 * will have a section name like _xxxxx_*_subscriber_*_ and _xxxxx_broker_subscriber* will get
 * sorted in front of _xxxxx_subscriber*. There will be problems if there are ever brokers with the
 * same name.
 *
 * @param _name The name of the defined broker
 * @param _work_q The work queue the broker is to run on
 */
#define PUB_SUB_BROKER_DEFINE(_name, _work_q)                                                      \
	PUB_SUB_ADD_BROKER_ENTRY(_CONCAT(_broker_entry_, _name), NULL, _CONCAT(_name, _broker),    \
				 0);                                                               \
	STRUCT_SECTION_ITERABLE(pub_sub_broker, _name) =                                           \
		PUB_SUB_BROKER_INITIALIZER(_name, _work_q, &_CONCAT(_broker_entry_, _name))

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
 * @param _subs_bitarray The subscriptions bitarray to use to track subscriptions
 * @param _max_msg_id The maximum message id that the subscriber will subscribe to
 */
#define PUB_SUB_SUBSCRIBER_INITIALIZER(_self, _work_q, _handler_fn, _subs_bitarray, _max_msg_id)   \
	{                                                                                          \
		.work = Z_WORK_INITIALIZER(pub_sub_subscriber_work_handler), .work_q = _work_q,    \
		.msg_handler = _handler_fn, .subs_bitarray = _subs_bitarray,                       \
		.fifo = Z_FIFO_INITIALIZER(_self.fifo),                                            \
		COND_CODE_1(CONFIG_PUB_SUB_RUNTIME_SUBSCRIBERS,                                    \
			    (.broker = NULL, .sub_list_node = {.next = NULL}, .priority = 0, ),    \
			    (.broker_entry = NULL, ))                                              \
			.max_pub_msg_id = _max_msg_id,                                             \
	}

/**
 * @brief Add a subscriber component to another struct
 *
 * @param _max_msg_id The maximum message id that the subscriber will subscribe to
 */
#define PUB_SUB_ADD_SUBSCRIBER_CMPNT(_max_msg_id)                                                  \
	struct pub_sub_subscriber _subscriber;                                                     \
	PUB_SUB_SUBS_BITARRAY_DEFINE(_subs_bitarray, _max_msg_id)

/**
 * @brief Statically initialize a subscriber that is a component of another struct
 *
 * @param _composite The composite struct the subscriber belongs to
 * @param _work_q The work queue the subscriber is to run on
 * @param _handler_fn The message handler function of the subscriber
 * @param _max_msg_id The maximum message id that the subscriber will subscribe to
 */
#define PUB_SUB_INIT_SUBSCRIBER_CMPNT(_composite, _work_q, _handler_fn, _max_msg_id)               \
	._subscriber =                                                                             \
		PUB_SUB_SUBSCRIBER_INITIALIZER(_composite._subscriber, _work_q, _handler_fn,       \
					       _composite._subs_bitarray, _max_msg_id),            \
	._subs_bitarray = {}

/**
 * @brief Retrieve a pointer to the component subscriber from the composite struct
 *
 * @param _composite The struct to get the subscriber pointer from
 */
#define PUB_SUB_SUBSCRIBER_CMPNT(_composite) (&((_composite)->_subscriber))

/**
 * @brief Retrieve a pointer to the composite struct from a component subscriber pointer
 *
 * @param _ptr A pointer to the subscriber
 * @param _type The name of the type of the composite struct
 */
#define PUB_SUB_CONTAINER_FROM_SUBSCRIBER(_ptr, _type) CONTAINER_OF(_ptr, _type, _subscriber)

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
 * the message is already in the subscriber's fifo
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
 * Only private messages (message id greater than the subscriber's max public id) can be published
 * directly to a subscriber. It bypasses the subscriber's subscription list and is always received.
 *
 * Publishing a message passes ownership of the message's reference to the pub_sub framework i.e.
 * after publish is called the memory pointed to by 'msg' should not be accessed again. A message
 * can only be published to a single subscriber even if multiple references are owned.
 *
 * @param subscriber Address of the subscriber to publish to
 * @param msg Address of the message to publish
 */
static inline void pub_sub_publish_to_subscriber(struct pub_sub_subscriber *subscriber,
						 const void *msg)
{
	__ASSERT(subscriber != NULL, "");
	__ASSERT(pub_sub_msg_get_msg_id(msg) > subscriber->max_pub_msg_id,
		 "Public messages can not be published directly  to a subscriber");
	pub_sub_msg_fifo_put(&subscriber->fifo, msg);
	k_work_submit_to_queue(subscriber->work_q, &subscriber->work);
}

#ifdef CONFIG_PUB_SUB_DEFAULT_BROKER

extern struct pub_sub_broker g_pub_sub_default_broker;

/**
 * @brief Statically add a subscriber to the default broker
 *
 * This macro creates a pub_sub_broker_subscriber_entry for the subscriber in the default broker's
 * iterable section.
 *
 * @param _subscriber_ptr Address of the subscriber to add to the broker
 * @param _priority The priority of the subscriber, 0 is highest priority, 99 is lowest priority
 */
#define PUB_SUB_SUBSCRIBER_ADD(_subscriber_ptr, _priority)                                         \
	PUB_SUB_SUBSCRIBER_ADD_TO_BROKER(_subscriber_ptr, g_pub_sub_default_broker, _priority)

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