/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PUB_SUB_STATIC_MSG_H_
#define PUB_SUB_STATIC_MSG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <pub_sub/msg_alloc.h>
#include <zephyr/sys/check.h>

typedef void (*pub_sub_msg_callback_fn)(const void *msg);

struct pub_sub_msg_callback {
	pub_sub_msg_callback_fn callback;
	// Must be last as the user msg follows
	struct pub_sub_msg pub_sub_msg;
};

/**
 * @brief Wraps a message struct so that it can be used as a publish subscribe message
 *
 * @param struct_name The name to give the wrapped struct
 * @param msg_type The type of the message to wrap
 */
#define PUB_SUB_WRAP_STATIC_MSG(struct_name, msg_type)                                             \
	struct struct_name {                                                                       \
		struct pub_sub_msg pub_sub_msg;                                                    \
		msg_type msg;                                                                      \
	}

/**
 * @brief Statically define and initialize a static publish subscribe message
 *
 * @param msg_type The type of the message
 * @param var_name The name of the created message variable
 * @param msg_id The message id to initialize the message with
 */
#define PUB_SUB_STATIC_MSG_DEFINE(msg_type, var_name, msg_id)                                      \
	PUB_SUB_WRAP_STATIC_MSG(_static_msg_wrapped_##var_name, msg_type);                         \
	static struct _static_msg_wrapped_##var_name _static_msg_wrapped_##var_name = {            \
		.pub_sub_msg.atomic_data =                                                         \
			PUB_SUB_MSG_ATOMIC_DATA_INIT(msg_id, PUB_SUB_ALLOC_ID_STATIC_MSG)};        \
	static msg_type *var_name = &_static_msg_wrapped_##var_name.msg

/**
 * @brief Wraps a message struct so that it can be used as a callback publish subscribe message
 *
 * @param struct_name The name to give the wrapped struct
 * @param msg_type The type of the message to wrap
 */
#define PUB_SUB_WRAP_CALLBACK_MSG(struct_name, msg_type)                                           \
	struct struct_name {                                                                       \
		struct pub_sub_msg_callback callback_msg;                                          \
		msg_type msg;                                                                      \
	}

/**
 * @brief Statically define and initialize a callback publish subscribe message
 *
 * @param msg_type The type of the message
 * @param var_name The name of the created message variable
 * @param msg_id The message id to initialize the message with
 * @param callback_fn The callback function to initialize the message with
 */
#define PUB_SUB_STATIC_CALLBACK_MSG_DEFINE(msg_type, var_name, msg_id, callback_fn)                \
	PUB_SUB_WRAP_CALLBACK_MSG(_callback_msg_wrapped_##var_name, msg_type);                     \
	static struct _callback_msg_wrapped_##var_name _callback_msg_wrapped_##var_name = {        \
		.callback_msg = {.callback = callback_fn,                                          \
				 .pub_sub_msg.atomic_data = PUB_SUB_MSG_ATOMIC_DATA_INIT(          \
					 msg_id, PUB_SUB_ALLOC_ID_CALLBACK_MSG)},                  \
	};                                                                                         \
	static msg_type *var_name = &_callback_msg_wrapped_##var_name.msg

/**
 * @brief Initialize a static publish subscribe message
 *
 * Initializes the message's reference counter to 1 and sets its message id to the
 * passed in value.
 *
 * @warning
 * Must only be called with messages that conform to the publish subscribe message memory layout
 * i.e. the message is preceded by the pub_sub_msg struct.
 *
 * @param msg Address of the message to initialize
 * @param msg_id The message id to initialize the message with
 */
static inline void pub_sub_static_msg_init(void *msg, uint16_t msg_id)
{
	__ASSERT(msg != NULL, "");
	pub_sub_msg_init(msg, msg_id, PUB_SUB_ALLOC_ID_STATIC_MSG);
}

/**
 * @brief Initialize a static callback publish subscribe message
 *
 * Initializes the message's reference counter to 1 and sets its message id and callback function to
 * the passed in values.
 *
 * @warning
 * Must only be called with messages that conform to the callback publish subscribe message memory
 * layout i.e. the message is preceded by the pub_sub_msg_callback struct.
 *
 * @param msg Address of the message to initialize
 * @param msg_id The message id to initialize the message with
 * @param callback The callback function to initialize the message with
 */
static inline void pub_sub_callback_msg_init(void *msg, uint16_t msg_id,
					     pub_sub_msg_callback_fn callback)
{
	__ASSERT(msg != NULL, "");
	__ASSERT(callback != NULL, "");
	struct pub_sub_msg *ps_msg = CONTAINER_OF(msg, struct pub_sub_msg, msg);
	struct pub_sub_msg_callback *cb_msg =
		CONTAINER_OF(ps_msg, struct pub_sub_msg_callback, pub_sub_msg);
	cb_msg->callback = callback;
	pub_sub_msg_init(msg, msg_id, PUB_SUB_ALLOC_ID_CALLBACK_MSG);
}

/**
 * @brief Free a static callback publish subscribe message
 *
 * This function is used by the publish subscribe framework when a callback message's reference
 * counter reaches 0. It calls the message's callback to notify the message's owner that the message
 * is now free to be re-used.
 *
 * @warning
 * Must only be called with messages that conform to the callback publish subscribe message memory
 * layout i.e. the message is preceded by the pub_sub_msg_callback struct.
 *
 * @param msg Address of the message
 */
static inline void pub_sub_free_callback_msg(const void *msg)
{
	__ASSERT(msg != NULL, "");
	struct pub_sub_msg *ps_msg = CONTAINER_OF(msg, struct pub_sub_msg, msg);
	struct pub_sub_msg_callback *cb_msg =
		CONTAINER_OF(ps_msg, struct pub_sub_msg_callback, pub_sub_msg);
	__ASSERT(cb_msg->callback != NULL, "");
	cb_msg->callback(msg);
}

#ifdef __cplusplus
}
#endif

#endif /* PUB_SUB_STATIC_MSG_H_ */