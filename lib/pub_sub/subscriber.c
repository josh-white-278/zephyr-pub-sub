/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <pub_sub/pub_sub.h>
#include <string.h>

static void common_subscriber_init(struct pub_sub_subscriber *subscriber, atomic_t *subs_bitarray,
				   uint16_t max_pub_msg_ids);
static void send_to_next_fifo_subscriber(struct pub_sub_subscriber *subscriber, uint16_t msg_id,
					 void *msg);

void pub_sub_init_callback_subscriber(struct pub_sub_subscriber *subscriber,
				      atomic_t *subs_bitarray, uint16_t max_pub_msg_id)
{
	__ASSERT(subscriber != NULL, "");
	__ASSERT(subs_bitarray != NULL, "");
	common_subscriber_init(subscriber, subs_bitarray, max_pub_msg_id);
	subscriber->rx_type = PUB_SUB_RX_TYPE_CALLBACK;
}

void pub_sub_init_msgq_subscriber(struct pub_sub_subscriber *subscriber, atomic_t *subs_bitarray,
				  uint16_t max_pub_msg_id, struct k_msgq *msgq)
{
	__ASSERT(subscriber != NULL, "");
	__ASSERT(msgq != NULL, "");
	__ASSERT(subs_bitarray != NULL, "");
	common_subscriber_init(subscriber, subs_bitarray, max_pub_msg_id);
	subscriber->msgq = msgq;
	subscriber->rx_type = PUB_SUB_RX_TYPE_MSGQ;
}

void pub_sub_init_fifo_subscriber(struct pub_sub_subscriber *subscriber, atomic_t *subs_bitarray,
				  uint16_t max_pub_msg_id)
{
	__ASSERT(subscriber != NULL, "");
	__ASSERT(subs_bitarray != NULL, "");
	common_subscriber_init(subscriber, subs_bitarray, max_pub_msg_id);
	k_fifo_init(&subscriber->fifo);
	subscriber->rx_type = PUB_SUB_RX_TYPE_FIFO;
}

int pub_sub_populate_poll_evt(struct pub_sub_subscriber *subscriber, struct k_poll_event *poll_evt)
{
	__ASSERT(subscriber != NULL, "");
	__ASSERT(poll_evt != NULL, "");
	int ret = 0;
	switch (subscriber->rx_type) {
	case PUB_SUB_RX_TYPE_CALLBACK: {
		ret = -EPERM;
		break;
	}
	case PUB_SUB_RX_TYPE_MSGQ: {
		k_poll_event_init(poll_evt, K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
				  K_POLL_MODE_NOTIFY_ONLY, subscriber->msgq);
		break;
	}
	case PUB_SUB_RX_TYPE_FIFO: {
		k_poll_event_init(poll_evt, K_POLL_TYPE_FIFO_DATA_AVAILABLE,
				  K_POLL_MODE_NOTIFY_ONLY, &subscriber->fifo);
		break;
	}
	}
	return ret;
}

int pub_sub_handle_queued_msg(struct pub_sub_subscriber *subscriber, k_timeout_t timeout)
{
	__ASSERT(subscriber != NULL, "");

	int ret = -ENOMSG;
	switch (subscriber->rx_type) {
	case PUB_SUB_RX_TYPE_CALLBACK: {
		ret = -EPERM;
		break;
	}
	case PUB_SUB_RX_TYPE_MSGQ: {
		void *msg;
		ret = k_msgq_get(subscriber->msgq, &msg, timeout);
		if (ret == 0) {
			uint16_t msg_id = pub_sub_msg_get_msg_id(msg);
			__ASSERT(subscriber->handler_data.msg_handler != NULL, "");
			subscriber->handler_data.msg_handler(msg_id, msg,
							     subscriber->handler_data.user_data);
			pub_sub_release_msg(msg);
		}
		break;
	}
	case PUB_SUB_RX_TYPE_FIFO: {
		void *msg = pub_sub_msg_fifo_get(&subscriber->fifo, timeout);
		if (msg != NULL) {
			uint16_t msg_id = pub_sub_msg_get_msg_id(msg);
			ret = 0;
			__ASSERT(subscriber->handler_data.msg_handler != NULL, "");
			// If it is a public message pass it to any other fifo subscribers further
			// down the list then handle the message
			if (msg_id <= subscriber->max_pub_msg_id) {
				send_to_next_fifo_subscriber(subscriber, msg_id, msg);
			}
			subscriber->handler_data.msg_handler(msg_id, msg,
							     subscriber->handler_data.user_data);
			pub_sub_release_msg(msg);
		}
		break;
	}
	}
	return ret;
}

void pub_sub_publish_to_subscriber(struct pub_sub_subscriber *subscriber, void *msg)
{
	__ASSERT(subscriber != NULL, "");
	__ASSERT(pub_sub_msg_get_msg_id(msg) > subscriber->max_pub_msg_id,
		 "Public messages can not be published directly to subscriber");
	switch (subscriber->rx_type) {
	case PUB_SUB_RX_TYPE_CALLBACK: {
		__ASSERT(subscriber->handler_data.msg_handler != NULL, "");
		uint16_t msg_id = pub_sub_msg_get_msg_id(msg);
		subscriber->handler_data.msg_handler(msg_id, msg,
						     subscriber->handler_data.user_data);
		pub_sub_release_msg(msg);
		break;
	}
	case PUB_SUB_RX_TYPE_MSGQ: {
		k_msgq_put(subscriber->msgq, &msg, K_FOREVER);
		break;
	}
	case PUB_SUB_RX_TYPE_FIFO: {
		pub_sub_msg_fifo_put(&subscriber->fifo, msg);
		break;
	}
	}
}

static void common_subscriber_init(struct pub_sub_subscriber *subscriber, atomic_t *subs_bitarray,
				   uint16_t max_pub_msg_id)
{
	memset(subs_bitarray, 0, PUB_SUB_SUBS_BITARRAY_BYTE_LEN(max_pub_msg_id));
	subscriber->broker = NULL;
	subscriber->subs_bitarray = subs_bitarray;
	subscriber->max_pub_msg_id = max_pub_msg_id;
	subscriber->priority = 0;
}

// This function assumes that 'subscriber' is also a fifo subscriber
static void send_to_next_fifo_subscriber(struct pub_sub_subscriber *subscriber, uint16_t msg_id,
					 void *msg)
{
	struct pub_sub_broker *broker = subscriber->broker;
	k_mutex_lock(&broker->sub_list_mutex, K_FOREVER);
	// fifo subscribers are at the end of the list so we can just iterate until we hit either a
	// subscription or the end of the list
	for (struct pub_sub_subscriber *next_sub =
		     SYS_SLIST_PEEK_NEXT_CONTAINER(subscriber, sub_list_node);
	     next_sub != NULL; next_sub = SYS_SLIST_PEEK_NEXT_CONTAINER(next_sub, sub_list_node)) {
		if ((msg_id <= next_sub->max_pub_msg_id) &&
		    atomic_test_bit(next_sub->subs_bitarray, msg_id)) {
			pub_sub_acquire_msg(msg);
			pub_sub_msg_fifo_put(&next_sub->fifo, msg);
			break;
		}
	}
	k_mutex_unlock(&broker->sub_list_mutex);
}