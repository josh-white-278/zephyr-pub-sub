/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef HSM_HSM_SUBSCRIBER_H_
#define HSM_HSM_SUBSCRIBER_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <pub_sub/pub_sub.h>
#include <hsm/hsm.h>

struct hsm_subscriber {
	struct hsm hsm;
	struct pub_sub_subscriber subscriber;
};

/**
 * @brief Compose a hsm_subscriber into another struct
 *
 * @param _max_msg_id The maximum message id that the subscriber will subscribe to
 */
#define HSM_SUB_COMPOSE(_max_msg_id)                                                               \
	struct hsm_subscriber _hsm_subscriber;                                                     \
	PUB_SUB_SUBS_BITARRAY_DEFINE(_subs_bitarray, _max_msg_id)

/**
 * @brief Statically initialize a hsm_subscriber that was composed into another struct with
 * HSM_SUB_COMPOSE
 *
 * @param _composite The composite struct the hsm_subscriber belongs to
 * @param _work_q The work queue the subscriber is to run on
 * @param _max_msg_id The maximum message id that the subscriber will subscribe to
 * @param _priority The priority value to set
 */
#define HSM_SUB_INIT_COMPOSED(_composite, _work_q, _max_msg_id, _priority)                         \
	._hsm_subscriber =                                                                         \
		{                                                                                  \
			.hsm = {},                                                                 \
			.subscriber = PUB_SUB_SUBSCRIBER_INITIALIZER(                              \
				_composite._hsm_subscriber.subscriber, _work_q,                    \
				hsm_subscriber_msg_handler, _composite._subs_bitarray,             \
				_max_msg_id, _priority),                                           \
	},                                                                                         \
	._subs_bitarray = {}

/**
 * @brief Retrieve a pointer to the composed hsm_subscriber's subscriber from the composite struct
 *
 * @param _composite The struct to get the subscriber pointer from
 */
#define HSM_SUB_COMPOSED_SUBSCRIBER_PTR(_composite) (&(_composite)._hsm_subscriber.subscriber)

/**
 * @brief Retrieve a pointer to the composite struct from a composed hsm_subscriber's subscriber
 * pointer
 *
 * @param _ptr A pointer to the subscriber
 * @param _type The name of the type of the composite struct
 */
#define HSM_SUB_CONTAINER_FROM_SUBSCRIBER(_ptr, _type)                                             \
	(CONTAINER_OF(_ptr, _type, _hsm_subscriber.subscriber))

/**
 * @brief Retrieve a pointer to the composed hsm_subscriber's hsm from the composite struct
 *
 * @param _composite The struct to get the hsm pointer from
 */
#define HSM_SUB_COMPOSED_HSM_PTR(_composite) (&(_composite)._hsm_subscriber.hsm)

/**
 * @brief Retrieve a pointer to the composite struct from a composed hsm_subscriber's hsm pointer
 *
 * @param _ptr A pointer to the hsm
 * @param _type The name of the type of the composite struct
 */
#define HSM_SUB_CONTAINER_FROM_HSM(_ptr, _type) (CONTAINER_OF(_ptr, _type, _hsm_subscriber.hsm))

// Message handler function for hsm_subscriber, just calls hsm_run
static inline void hsm_subscriber_msg_handler(struct pub_sub_subscriber *subscriber,
					      uint16_t msg_id, const void *msg)
{
	struct hsm_subscriber *hsm_subscriber =
		CONTAINER_OF(subscriber, struct hsm_subscriber, subscriber);
	hsm_run(&hsm_subscriber->hsm, msg_id, msg);
}

#ifdef __cplusplus
}
#endif

#endif /* HSM_HSM_SUBSCRIBER_H_ */