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
	struct _timeout timeout;
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
		.delayable_msg = {.timeout =                                                       \
					  {                                                        \
						  .node = {},                                      \
						  .fn = pub_sub_delayable_msg_handler,             \
						  .dticks = 0,                                     \
					  },                                                       \
				  .subscriber = _subscriber,                                       \
				  .pub_sub_msg.atomic_data = PUB_SUB_MSG_ATOMIC_DATA_INIT(         \
					  msg_id, PUB_SUB_ALLOC_ID_STATIC_MSG)},                   \
	};                                                                                         \
	static msg_type *var_name =                                                                \
		(msg_type *)&_delayable_msg_union_##var_name.delayable_msg.pub_sub_msg.msg

/**
 * @brief Internal implementation, only exposed for PUB_SUB_STATIC_DELAYABLE_MSG_DEFINE
 */
void pub_sub_delayable_msg_handler(struct _timeout *t);

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
 * @warning
 * Must only be called with messages that conform to the delayable message memory layout
 * i.e. the message is preceded by the pub_sub_msg_delayable struct.
 *
 * @param msg Address of the message to start
 * @param delay The time to wait before publishing the message
 */
void pub_sub_delayable_msg_start(const void *msg, k_timeout_t delay);

/**
 * @brief Update the timeout delay of a delayable publish subscribe message
 *
 * Internally this function aborts the timer and then starts it with the new delay.
 *
 * @warning
 * Must only be called with messages that conform to the delayable message memory layout
 * i.e. the message is preceded by the pub_sub_msg_delayable struct.
 *
 * @warning
 * Updating a delayable message will not remove it from the subscriber's message queue/fifo if it
 * has already timed out. If this function does not return 0 the subscriber will receive the
 * delayable message twice, the first for the old delay and the second for the just updated delay.
 *
 * @param msg Address of the message to update
 * @param delay The new time to wait before publishing the message
 *
 * @retval 0 if successfully updated
 * @retval -EINVAL if the message has already timed out but has not been handled yet
 */
int pub_sub_delayable_msg_update_timeout(const void *msg, k_timeout_t delay);

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
 * @retval -EINVAL if the message has already timed out but has not been handled yet
 */
int pub_sub_delayable_msg_abort(const void *msg);

#ifdef __cplusplus
}
#endif

#endif /* PUB_SUB_DELAYABLE_MSG_H_ */