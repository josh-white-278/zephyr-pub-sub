/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PUB_SUB_DELAYABLE_MSG_H_
#define PUB_SUB_DELAYABLE_MSG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <pub_sub/pub_sub.h>

struct pub_sub_delayable_msg_header {
	k_timepoint_t end_time;
	sys_dnode_t node;
	struct pub_sub_subscriber *subscriber;
	bool aborted;
	struct pub_sub_msg_header pub_sub_msg_header;
};

struct pub_sub_delayable_msg {
	struct pub_sub_delayable_msg_header header;
	uint8_t __aligned(sizeof(void *)) msg[];
};

/**
 * @brief Declares a struct that prepends a message struct with a delayable message header so that
 * the message struct can be used as a delayable message
 *
 * @param struct_name The name to give the declared struct
 * @param msg_struct The message to prepend with the delayable message header
 */
#define PUB_SUB_DELAYABLE_MSG_DECLARE(_struct_name, _msg_struct)                                   \
	struct _struct_name {                                                                      \
		struct pub_sub_delayable_msg_header header;                                        \
		_msg_struct __aligned(sizeof(void *)) msg;                                         \
	}

/**
 * @brief Get a pointer to the struct declared with PUB_SUB_DELAYABLE_MSG_DECLARE from a message
 * pointer as received in a subscriber message handler function. Can also use struct
 * pub_sub_delayable_msg as _struct_name to get a pointer to a generic delayable message.
 *
 * @param _struct_name The struct created by PUB_SUB_DELAYABLE_MSG_DECLARE
 * @param _msg The message pointer received
 */
#define PUB_SUB_MSG_TO_DELAYABLE_MSG(_struct_name, _msg) CONTAINER_OF(_msg, _struct_name, msg)

/**
 * @brief Cast a pointer to a delayable message declared with PUB_SUB_DELAYABLE_MSG_DECLARE to a
 * generic pub_sub_delayable_msg pointer. Also can be used with a struct pub_sub_timer_msg message.
 *
 * Uses CONTAINER_OF so there is a bit of type checking, i.e. _msg must have a header and it must be
 * of type struct pub_sub_delayable_msg_header
 *
 * @param _msg The message to cast to a pub_sub_delayable_msg
 */
#define PUB_SUB_DECLARED_TO_DELAYABLE_MSG(_msg)                                                    \
	CONTAINER_OF(&(_msg)->header, struct pub_sub_delayable_msg, header)

/**
 * @brief Statically initialize a delayable publish subscribe message header
 *
 * @param _msg_id The message id to initialize the message with, must be a private msg id
 * @param _subscriber The subscriber to publish to
 */
#define PUB_SUB_DELAYABLE_MSG_HEADER_INITIALIZER(_msg_id, _subscriber)                             \
	{                                                                                          \
		.node = {}, .subscriber = _subscriber, .aborted = false,                           \
		.pub_sub_msg_header = {                                                            \
			.atomic_data = PUB_SUB_MSG_ATOMIC_DATA_INIT(                               \
				_msg_id, PUB_SUB_ALLOC_ID_DELAYABLE_MSG),                          \
		},                                                                                 \
	}

/**
 * @brief A delayable message with no data i.e. a simple timer message
 */
struct pub_sub_timer_msg {
	struct pub_sub_delayable_msg_header header;
};

/**
 * @brief Statically initialize a pub_sub_timer_msg
 *
 * @param _msg_id The message id to initialize the message with, must be a private msg id
 * @param _subscriber The subscriber to publish to
 */
#define PUB_SUB_TIMER_MSG_INITIALIZER(_msg_id, _subscriber)                                        \
	{                                                                                          \
		.header = PUB_SUB_DELAYABLE_MSG_HEADER_INITIALIZER(_msg_id, _subscriber),          \
	}

/**
 * @brief Define and initialize a pub_sub_timer_msg
 *
 * @param name The name to give the defined pub_sub_timer_msg
 * @param _msg_id The message id to initialize the message with, must be a private msg id
 * @param _subscriber The subscriber to publish to
 */
#define PUB_SUB_TIMER_MSG_DEFINE(name, _msg_id, _subscriber)                                       \
	struct pub_sub_timer_msg name = PUB_SUB_TIMER_MSG_INITIALIZER(_msg_id, _subscriber)

/**
 * @brief Initialize a delayable publish subscribe message
 *
 * @param delayable_msg Address of the message to initialize
 * @param subscriber The subscriber that the message will be published to
 * @param msg_id The message id to initialize the message with, must be a private msg id
 */
void pub_sub_delayable_msg_init(struct pub_sub_delayable_msg *delayable_msg,
				struct pub_sub_subscriber *subscriber, uint16_t msg_id);

/**
 * @brief Start the timer on a delayable publish subscribe message
 *
 * Starting a delayable message that is already running is allowed, it is the same as aborting the
 * message and then starting it again.
 *
 * @warning
 * Restarting a delayable message will not remove it from the subscriber's fifo if it has already
 * timed out. If the message is already queued with the subscriber then this function will set the
 * message's internal aborted state to true which can be checked with the function
 * pub_sub_delayable_msg_was_aborted when the message is handled. pub_sub_delayable_msg_was_aborted
 * will return true until the the message is handled by the subscriber after which the internal
 * aborted state will be automatically cleared.
 *
 * @param delayable_msg Address of the message to start
 * @param timeout The time to wait before publishing the message
 */
void pub_sub_delayable_msg_start(struct pub_sub_delayable_msg *delayable_msg, k_timeout_t timeout);

/**
 * @brief Abort the publishing of a delayable publish subscribe message
 *
 * @warning
 * Aborting a delayable message will not remove it from the subscriber's fifo if it has already
 * timed out. If the message is already queued with the subscriber then this function will set the
 * message's internal aborted state to true which can be checked with the function
 * pub_sub_delayable_msg_was_aborted when the message is handled. pub_sub_delayable_msg_was_aborted
 * will return true until the the message is handled by the subscriber after which the internal
 * aborted state will be automatically cleared.
 *
 * @param delayable_msg Address of the message to abort
 */
void pub_sub_delayable_msg_abort(struct pub_sub_delayable_msg *delayable_msg);

/**
 * @brief Check if a delayable message has an active timeout
 *
 * @param delayable_msg Address of the message to check
 *
 * @return true if the message has an active timeout, false if it does not
 */
static inline bool pub_sub_delayable_msg_is_active(struct pub_sub_delayable_msg *delayable_msg)
{
	__ASSERT(delayable_msg != NULL, "");
	return sys_dnode_is_linked(&delayable_msg->header.node);
}

/**
 * @brief Check if a delayable message was aborted while queued with the subscriber
 *
 * @warning
 * Must only be called with messages that conform to the delayable message memory layout
 * i.e. the message is preceded by the pub_sub_delayable_msg_header struct.
 *
 * @param delayable_msg Address of the message to check
 *
 * @return true if the message was aborted, false otherwise
 */
static inline bool pub_sub_delayable_msg_was_aborted(struct pub_sub_delayable_msg *delayable_msg)
{
	__ASSERT(delayable_msg != NULL, "");
	return delayable_msg->header.aborted;
}

// Internal use
void pub_sub_free_delayable_msg(const void *msg);

#ifdef __cplusplus
}
#endif

#endif /* PUB_SUB_DELAYABLE_MSG_H_ */