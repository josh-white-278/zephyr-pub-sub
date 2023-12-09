/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <pub_sub/pub_sub.h>
#include <string.h>

static void common_subscriber_init(struct pub_sub_subscriber *subscriber, atomic_t *subs_bitarray,
				   size_t subs_bitarray_len_bytes);

void pub_sub_init_callback_subscriber(struct pub_sub_subscriber *subscriber,
				      atomic_t *subs_bitarray, size_t subs_bitarray_len_bytes)
{
	__ASSERT(subscriber != NULL, "");
	__ASSERT(subs_bitarray != NULL, "");
	common_subscriber_init(subscriber, subs_bitarray, subs_bitarray_len_bytes);
	subscriber->rx_type = PUB_SUB_RX_TYPE_CALLBACK;
}

void pub_sub_init_msgq_subscriber(struct pub_sub_subscriber *subscriber, atomic_t *subs_bitarray,
				  size_t subs_bitarray_len_bytes, struct k_msgq *msgq)
{
	__ASSERT(subscriber != NULL, "");
	__ASSERT(msgq != NULL, "");
	__ASSERT(subs_bitarray != NULL, "");
	common_subscriber_init(subscriber, subs_bitarray, subs_bitarray_len_bytes);
	subscriber->msgq = msgq;
	subscriber->rx_type = PUB_SUB_RX_TYPE_MSGQ;
}

void pub_sub_init_fifo_subscriber(struct pub_sub_subscriber *subscriber, atomic_t *subs_bitarray,
				  size_t subs_bitarray_len_bytes)
{
	__ASSERT(subscriber != NULL, "");
	__ASSERT(subs_bitarray != NULL, "");
	common_subscriber_init(subscriber, subs_bitarray, subs_bitarray_len_bytes);
	k_fifo_init(&subscriber->fifo);
	subscriber->rx_type = PUB_SUB_RX_TYPE_FIFO;
}

static void common_subscriber_init(struct pub_sub_subscriber *subscriber, atomic_t *subs_bitarray,
				   size_t subs_bitarray_len_bytes)
{
	memset(subs_bitarray, 0, subs_bitarray_len_bytes);
	subscriber->broker = NULL;
	subscriber->subs_bitarray = subs_bitarray;
	subscriber->priority = 0;
}