/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PUB_SUB_MSG_H_
#define PUB_SUB_MSG_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <zephyr/kernel.h>

BUILD_ASSERT(sizeof(atomic_t) >= 4);

#define PUB_SUB_MSG_ID_MASK      GENMASK(31, 16)
#define PUB_SUB_MSG_ALLOC_MASK   GENMASK(15, 8)
#define PUB_SUB_MSG_REF_CNT_MASK GENMASK(7, 0)

#define PUB_SUB_MSG_ATOMIC_DATA_INIT(msg_id, alloc_id)                                             \
	ATOMIC_INIT(FIELD_PREP(PUB_SUB_MSG_ID_MASK, msg_id) |                                      \
		    FIELD_PREP(PUB_SUB_MSG_ALLOC_MASK, alloc_id) |                                 \
		    FIELD_PREP(PUB_SUB_MSG_REF_CNT_MASK, 0))

#define PUB_SUB_MSG_OVERHEAD_NUM_BYTES (sizeof(struct pub_sub_msg))

struct pub_sub_msg {
	void *fifo_reserved;
	// To save space atomic data contains the msg id, allocator id and reference counter e.g.
	// uint16_t msg_id
	// uint8_t allocator_id
	// uint8_t ref_cnt
	atomic_t atomic_data;
	uint8_t __aligned(sizeof(void *)) msg[];
};

/**
 * @brief Initialize a publish subscribe message
 *
 * Initializes the message's reference counter to 1 and sets its message id and allocator id to the
 * passed in values.
 *
 * @warning
 * Must only be called with messages that conform to the publish subscribe message memory layout
 * i.e. the message is preceded by the pub_sub_msg struct.
 *
 * @param msg Address of the message to initialize
 * @param msg_id The message id to initialize the message with
 * @param alloc_id The allocator id to initialize the message with
 */
static inline void pub_sub_msg_init(void *msg, uint16_t msg_id, uint8_t alloc_id)
{
	__ASSERT(msg != NULL, "");
	struct pub_sub_msg *ps_msg = CONTAINER_OF(msg, struct pub_sub_msg, msg);
	ps_msg->atomic_data = PUB_SUB_MSG_ATOMIC_DATA_INIT(msg_id, alloc_id);
}

/**
 * @brief Get a publish subscribe message's reference counter value
 *
 * @warning
 * Must only be called with messages that conform to the publish subscribe message memory layout
 * i.e. the message is preceded by the pub_sub_msg struct.
 *
 * @param msg Address of the message
 *
 * @retval The reference count
 */
static inline uint8_t pub_sub_msg_get_ref_cnt(const void *msg)
{
	__ASSERT(msg != NULL, "");
	struct pub_sub_msg *ps_msg = CONTAINER_OF(msg, struct pub_sub_msg, msg);
	return FIELD_GET(PUB_SUB_MSG_REF_CNT_MASK, atomic_get(&ps_msg->atomic_data));
}

/**
 * @brief Increment a publish subscribe message's reference counter value
 *
 * @warning
 * Must only be called with messages that conform to the publish subscribe message memory layout
 * i.e. the message is preceded by the pub_sub_msg struct.
 *
 * @warning
 * The reference counter is only an 8 bit value so must be less than 255 prior to calling this
 * function
 *
 * @param msg Address of the message
 */
static inline void pub_sub_msg_inc_ref_cnt(const void *msg)
{
	__ASSERT(msg != NULL, "");
	__ASSERT(pub_sub_msg_get_ref_cnt(msg) < UINT8_MAX, "ref count overflow");
	struct pub_sub_msg *ps_msg = CONTAINER_OF(msg, struct pub_sub_msg, msg);
	atomic_inc(&ps_msg->atomic_data);
}

/**
 * @brief Decrement a publish subscribe message's reference counter value
 *
 * @warning
 * Must only be called with messages that conform to the publish subscribe message memory layout
 * i.e. the message is preceded by the pub_sub_msg struct.
 *
 * @param msg Address of the message
 *
 * @retval The previous reference counter value
 */
static inline uint8_t pub_sub_msg_dec_ref_cnt(const void *msg)
{
	__ASSERT(msg != NULL, "");
	__ASSERT(pub_sub_msg_get_ref_cnt(msg) > 0, "ref count underflow");
	struct pub_sub_msg *ps_msg = CONTAINER_OF(msg, struct pub_sub_msg, msg);
	return FIELD_GET(PUB_SUB_MSG_REF_CNT_MASK, atomic_dec(&ps_msg->atomic_data));
}

/**
 * @brief Get a publish subscribe message's message id
 *
 * @warning
 * Must only be called with messages that conform to the publish subscribe message memory layout
 * i.e. the message is preceded by the pub_sub_msg struct.
 *
 * @param msg Address of the message
 *
 * @retval The message id
 */
static inline uint16_t pub_sub_msg_get_msg_id(const void *msg)
{
	__ASSERT(msg != NULL, "");
	struct pub_sub_msg *ps_msg = CONTAINER_OF(msg, struct pub_sub_msg, msg);
	return FIELD_GET(PUB_SUB_MSG_ID_MASK, atomic_get(&ps_msg->atomic_data));
}

/**
 * @brief Get a publish subscribe message's allocator id
 *
 * @warning
 * Must only be called with messages that conform to the publish subscribe message memory layout
 * i.e. the message is preceded by the pub_sub_msg struct.
 *
 * @param msg Address of the message
 *
 * @retval The allocator id
 */
static inline uint8_t pub_sub_msg_get_alloc_id(const void *msg)
{
	__ASSERT(msg != NULL, "");
	struct pub_sub_msg *ps_msg = CONTAINER_OF(msg, struct pub_sub_msg, msg);
	return FIELD_GET(PUB_SUB_MSG_ALLOC_MASK, atomic_get(&ps_msg->atomic_data));
}

/**
 * @brief Put a publish subscribe message into a fifo
 *
 * @warning
 * Must only be called with messages that conform to the publish subscribe message memory layout
 * i.e. the message is preceded by the pub_sub_msg struct..
 *
 * @param fifo Address of the fifo
 * @param msg Address of the message
 */
static inline void pub_sub_msg_fifo_put(struct k_fifo *fifo, const void *msg)
{
	__ASSERT(msg != NULL, "");
	__ASSERT(fifo != NULL, "");
	struct pub_sub_msg *ps_msg = CONTAINER_OF(msg, struct pub_sub_msg, msg);
	k_fifo_put(fifo, ps_msg);
}

/**
 * @brief Get a publish subscribe message from a fifo
 *
 * @warning
 * Must only be called with messages that conform to the publish subscribe message memory layout
 * i.e. the message is preceded by the pub_sub_msg struct.
 *
 * @param fifo Address of the fifo
 * @param timeout How long to wait for a message to become free
 *
 * @retval Address of the message if successful, NULL on timeout
 */
static inline void *pub_sub_msg_fifo_get(struct k_fifo *fifo, k_timeout_t timeout)
{
	struct pub_sub_msg *ps_msg = (struct pub_sub_msg *)k_fifo_get(fifo, timeout);
	return ps_msg != NULL ? ps_msg->msg : NULL;
}

#ifdef __cplusplus
}
#endif

#endif /* PUB_SUB_MSG_ALLOC_H_ */