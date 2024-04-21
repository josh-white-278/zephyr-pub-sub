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

struct pub_sub_msg_delayable {
	k_timepoint_t end_time;
	sys_dnode_t node;
	struct pub_sub_subscriber *subscriber;
	// Must be last as the user msg follows
	struct pub_sub_msg pub_sub_msg;
};

/**
 * @brief Statically define and initialize a delayable publish subscribe message
 *
 * @param msg_type The type of the message
 * @param var_name The name of the created message variable
 * @param msg_id The message id to initialize the message with, must be a private msg id
 * @param _subscriber The subscriber to publish to
 */
#define PUB_SUB_STATIC_DELAYABLE_MSG_DEFINE(msg_type, var_name, msg_id, _subscriber)               \
	union _delayable_msg_union_##var_name {                                                    \
		struct pub_sub_msg_delayable delayable_msg;                                        \
		uint8_t _msg_array[offsetof(struct pub_sub_msg_delayable, pub_sub_msg.msg) +       \
				   sizeof(msg_type)];                                              \
	};                                                                                         \
	static union _delayable_msg_union_##var_name _delayable_msg_union_##var_name = {           \
		.delayable_msg = {.node = {},                                                      \
				  .subscriber = _subscriber,                                       \
				  .pub_sub_msg.atomic_data = PUB_SUB_MSG_ATOMIC_DATA_INIT(         \
					  msg_id, PUB_SUB_ALLOC_ID_STATIC_MSG)},                   \
	};                                                                                         \
	static msg_type *var_name =                                                                \
		(msg_type *)&_delayable_msg_union_##var_name.delayable_msg.pub_sub_msg.msg

/**
 * @brief Initialize a delayable publish subscribe message
 *
 * @warning
 * Must only be called with messages that conform to the delayable message memory layout
 * i.e. the message is preceded by the pub_sub_msg_delayable struct.
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
 * @note This function will return -1 when starting the delayable message from the subscriber's
 * message handler function when handling the delayable message itself. This is because the -1
 * return value is based on the message's reference counter being greater than 0 which it always is
 * when the subscriber is handling the message. In this case the return value of this function can
 * be ignored and the double reception warning below does not apply.
 *
 * @warning
 * Must only be called with messages that conform to the delayable message memory layout
 * i.e. the message is preceded by the pub_sub_msg_delayable struct. Additionally the delayable
 * message must be initialized with pub_sub_delayable_msg_init() before being started.
 *
 * @warning
 * Starting a delayable message will not remove it from the subscriber's message queue/fifo if it
 * has already timed out. If this function does not return 0 the subscriber will receive the
 * delayable message twice, the first for the old timeout and the second for the just started
 * timeout.
 *
 * @param msg Address of the message to start
 * @param timeout The time to wait before publishing the message
 *
 * @retval 0 if successfully updated
 * @retval -EBUSY if the message has already timed out but has not been handled yet
 */
int pub_sub_delayable_msg_start(const void *msg, k_timeout_t timeout);

/**
 * @brief Abort the publishing of a delayable publish subscribe message
 *
 * @warning
 * Must only be called with messages that conform to the delayable message memory layout
 * i.e. the message is preceded by the pub_sub_msg_delayable struct.
 *
 * @warning
 * Aborting a delayable message will not remove it from the subscriber's message queue/fifo if it
 * has already timed out. If this function does not return 0 the subscriber will receive the
 * delayable message some time in the future.
 *
 * @param msg Address of the message to abort
 *
 * @retval 0 if successfully aborted
 * @retval -EBUSY if the message has already timed out but has not been handled yet
 */
int pub_sub_delayable_msg_abort(const void *msg);

/**
 * @brief Check if a delayable message has an active timeout
 *
 * @warning
 * Must only be called with messages that conform to the delayable message memory layout
 * i.e. the message is preceded by the pub_sub_msg_delayable struct.
 *
 * @param msg Address of the message to check
 *
 * @return true if the message has an active timeout, false if it does not
 */
bool pub_sub_delayable_msg_is_active(const void *msg);

#ifdef __cplusplus
}
#endif

#endif /* PUB_SUB_DELAYABLE_MSG_H_ */