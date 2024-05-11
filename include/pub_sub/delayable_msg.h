/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PUB_SUB_DELAYABLE_MSG_H_
#define PUB_SUB_DELAYABLE_MSG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <pub_sub/msg_alloc.h>
#include <pub_sub/subscriber.h>

struct pub_sub_msg_delayable_header {
	k_timepoint_t end_time;
	sys_dnode_t node;
	struct pub_sub_subscriber *subscriber;
	bool aborted;
	struct pub_sub_msg_header pub_sub_msg_header;
};

struct pub_sub_msg_delayable {
	struct pub_sub_msg_delayable_header header;
	uint8_t __aligned(sizeof(void *)) msg[];
};

#define PUB_SUB_WRAP_DELAYABLE_MSG(struct_name, msg_type)                                          \
	struct struct_name {                                                                       \
		struct pub_sub_msg_delayable_header _reserved;                                     \
		msg_type __aligned(sizeof(void *)) msg;                                            \
	}

/**
 * @brief Statically define and initialize a delayable publish subscribe message
 *
 * @param msg_type The type of the message
 * @param var_name The name of the created message variable
 * @param msg_id The message id to initialize the message with, must be a private msg id
 * @param _subscriber The subscriber to publish to
 */
#define PUB_SUB_STATIC_DELAYABLE_MSG_DEFINE(msg_type, var_name, msg_id, _subscriber)               \
	PUB_SUB_WRAP_DELAYABLE_MSG(_delayable_msg_wrapped_##var_name, msg_type);                   \
	static struct _delayable_msg_wrapped_##var_name _delayable_msg_wrapped_##var_name = {      \
		._reserved =                                                                       \
			{                                                                          \
				.node = {},                                                        \
				.subscriber = _subscriber,                                         \
				.aborted = false,                                                  \
				.pub_sub_msg_header =                                              \
					{                                                          \
						.atomic_data = PUB_SUB_MSG_ATOMIC_DATA_INIT(       \
							msg_id, PUB_SUB_ALLOC_ID_DELAYABLE_MSG),   \
					},                                                         \
			},                                                                         \
	};                                                                                         \
	static msg_type *var_name = &_delayable_msg_wrapped_##var_name.msg

/**
 * @brief Initialize a delayable publish subscribe message
 *
 * @warning
 * Must only be called with messages that conform to the delayable message memory layout i.e. the
 * message is preceded by the pub_sub_msg_delayable_header struct.
 *
 * @param msg Address of the message to initialize
 * @param subscriber The subscriber that the message will be published to
 * @param msg_id The message id to initialize the message with, must be a private msg id
 */
void pub_sub_delayable_msg_init(void *msg, struct pub_sub_subscriber *subscriber, uint16_t msg_id);

/**
 * @brief Start the timer on a delayable publish subscribe message
 *
 * Starting a delayable message that is already running is allowed, it is the same as aborting the
 * message and then starting it again.
 *
 * @warning
 * Must only be called with messages that conform to the delayable message memory layout
 * i.e. the message is preceded by the pub_sub_msg_delayable_header struct. Additionally the
 * delayable message must be initialized before being started.
 *
 * @warning
 * Restarting a delayable message will not remove it from the subscriber's message queue/fifo
 * if it has already timed out. If the message is already queued with the subscriber then this
 * function will set the message's internal aborted state to true which can be checked with the
 * function pub_sub_delayable_msg_was_aborted when the message is handled.
 * pub_sub_delayable_msg_was_aborted will return true until the the message is handled by the
 * subscriber after which the internal aborted state will be automatically cleared.
 *
 * @param msg Address of the message to start
 * @param timeout The time to wait before publishing the message
 */
void pub_sub_delayable_msg_start(const void *msg, k_timeout_t timeout);

/**
 * @brief Abort the publishing of a delayable publish subscribe message
 *
 * @warning
 * Must only be called with messages that conform to the delayable message memory layout
 * i.e. the message is preceded by the pub_sub_msg_delayable_header struct.
 *
 * @warning
 * Aborting a delayable message will not remove it from the subscriber's message queue/fifo
 * if it has already timed out. If the message is already queued with the subscriber then this
 * function will set the message's internal aborted state to true which can be checked with the
 * function pub_sub_delayable_msg_was_aborted when the message is handled.
 * pub_sub_delayable_msg_was_aborted will return true until the the message is handled by the
 * subscriber after which the internal aborted state will be automatically cleared.
 *
 * @param msg Address of the message to abort
 */
void pub_sub_delayable_msg_abort(const void *msg);

/**
 * @brief Check if a delayable message has an active timeout
 *
 * @warning
 * Must only be called with messages that conform to the delayable message memory layout
 * i.e. the message is preceded by the pub_sub_msg_delayable_header struct.
 *
 * @param msg Address of the message to check
 *
 * @return true if the message has an active timeout, false if it does not
 */
static inline bool pub_sub_delayable_msg_is_active(const void *msg)
{
	__ASSERT(msg != NULL, "");
	const struct pub_sub_msg_delayable *delayable_msg =
		CONTAINER_OF(msg, struct pub_sub_msg_delayable, msg);
	return sys_dnode_is_linked(&delayable_msg->header.node);
}

/**
 * @brief Check if a delayable message was aborted while queued with the subscriber
 *
 * @warning
 * Must only be called with messages that conform to the delayable message memory layout
 * i.e. the message is preceded by the pub_sub_msg_delayable_header struct.
 *
 * @param msg Address of the message to check
 *
 * @return true if the message was aborted, false otherwise
 */
static inline bool pub_sub_delayable_msg_was_aborted(const void *msg)
{
	__ASSERT(msg != NULL, "");
	const struct pub_sub_msg_delayable *delayable_msg =
		CONTAINER_OF(msg, struct pub_sub_msg_delayable, msg);
	return delayable_msg->header.aborted;
}

// Internal use
void pub_sub_free_delayable_msg(const void *msg);

#ifdef __cplusplus
}
#endif

#endif /* PUB_SUB_DELAYABLE_MSG_H_ */