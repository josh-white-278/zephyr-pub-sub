/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <pub_sub/pub_sub.h>
#include <zephyr/init.h>
#include <string.h>

static void process_public_msg(struct pub_sub_broker *broker, struct pub_sub_subscriber *subscriber,
			       uint16_t msg_id, void *msg, struct k_work_q *current_work_q);
static struct pub_sub_subscriber *get_next_subscriber(struct pub_sub_broker *broker,
						      struct pub_sub_subscriber *subscriber,
						      uint16_t msg_id);

void pub_sub_init_broker(struct pub_sub_broker *broker, struct k_work_q *work_q)
{
	__ASSERT(broker != NULL, "");
	__ASSERT(work_q != NULL, "");
	k_work_init(&broker->work, pub_sub_broker_work_handler);
	broker->work_q = work_q;
	k_fifo_init(&broker->msg_fifo);
	k_mutex_init(&broker->sub_list_mutex);
	sys_slist_init(&broker->subscribers);
}

void pub_sub_init_subscriber(struct pub_sub_subscriber *subscriber, struct k_work_q *work_q,
			     pub_sub_handler_fn msg_handler, atomic_t *subs_bitarray,
			     uint16_t max_pub_msg_id, uint8_t priority)
{
	__ASSERT(subscriber != NULL, "");
	__ASSERT(work_q != NULL, "");
	__ASSERT(msg_handler != NULL, "");
	__ASSERT(subs_bitarray != NULL, "");
	memset(subs_bitarray, 0, PUB_SUB_SUBS_BITARRAY_BYTE_LEN(max_pub_msg_id));
	k_work_init(&subscriber->work, pub_sub_subscriber_work_handler);
	subscriber->work_q = work_q;
	subscriber->broker = NULL;
	subscriber->sub_list_node.next = NULL;
	subscriber->msg_handler = msg_handler;
	subscriber->subs_bitarray = subs_bitarray;
	k_fifo_init(&subscriber->fifo);
	subscriber->max_pub_msg_id = max_pub_msg_id;
	subscriber->priority = priority;
}

void pub_sub_add_subscriber_to_broker(struct pub_sub_broker *broker,
				      struct pub_sub_subscriber *subscriber)
{
	__ASSERT(broker != NULL, "");
	__ASSERT(subscriber != NULL, "");
	__ASSERT(subscriber->msg_handler != NULL, "");
	__ASSERT(subscriber->broker == NULL, "");
	sys_snode_t *prev_node = NULL;
	struct pub_sub_subscriber *current = NULL;
	subscriber->broker = broker;
	// Subscribers need to be sorted by priority in the linked list. Iterate until we find a
	// node with a larger priority value than the new one then insert the new subscriber just
	// before the subscriber with the larger priority value
	k_mutex_lock(&broker->sub_list_mutex, K_FOREVER);
	current = SYS_SLIST_PEEK_HEAD_CONTAINER(&broker->subscribers, current, sub_list_node);
	while (current != NULL) {
		if (current->priority > subscriber->priority) {
			break;
		}
		prev_node = &current->sub_list_node;
		current = SYS_SLIST_PEEK_NEXT_CONTAINER(current, sub_list_node);
	}

	sys_slist_insert(&broker->subscribers, prev_node, &subscriber->sub_list_node);
	k_mutex_unlock(&broker->sub_list_mutex);
}

void pub_sub_subscriber_remove_broker(struct pub_sub_subscriber *subscriber)
{
	__ASSERT(subscriber != NULL, "");
	__ASSERT(subscriber->broker != NULL, "");
	struct pub_sub_broker *broker = subscriber->broker;
	k_mutex_lock(&broker->sub_list_mutex, K_FOREVER);
	sys_slist_find_and_remove(&broker->subscribers, &subscriber->sub_list_node);
	k_mutex_unlock(&broker->sub_list_mutex);
	subscriber->broker = NULL;
}

void pub_sub_broker_work_handler(struct k_work *work)
{
	struct pub_sub_broker *broker = CONTAINER_OF(work, struct pub_sub_broker, work);
	void *msg = pub_sub_msg_fifo_get(&broker->msg_fifo, K_NO_WAIT);
	while (msg != NULL) {
		uint16_t msg_id = pub_sub_msg_get_msg_id(msg);
		process_public_msg(broker, NULL, msg_id, msg, broker->work_q);
		pub_sub_release_msg(msg);
		msg = pub_sub_msg_fifo_get(&broker->msg_fifo, K_NO_WAIT);
	}
}

void pub_sub_subscriber_work_handler(struct k_work *work)
{
	struct pub_sub_subscriber *subscriber = CONTAINER_OF(work, struct pub_sub_subscriber, work);
	void *msg = pub_sub_msg_fifo_get(&subscriber->fifo, K_NO_WAIT);
	while (msg != NULL) {
		uint16_t msg_id = pub_sub_msg_get_msg_id(msg);
		__ASSERT(subscriber->msg_handler != NULL, "");
		subscriber->msg_handler(subscriber, msg_id, msg);
		// If it is a public message pass it to any other subscribers further down the list
		if (msg_id <= subscriber->max_pub_msg_id) {
			process_public_msg(subscriber->broker, subscriber, msg_id, msg,
					   subscriber->work_q);
		}
		pub_sub_release_msg(msg);
		msg = pub_sub_msg_fifo_get(&subscriber->fifo, K_NO_WAIT);
	}
}

static void process_public_msg(struct pub_sub_broker *broker, struct pub_sub_subscriber *subscriber,
			       uint16_t msg_id, void *msg, struct k_work_q *current_work_q)
{
	struct pub_sub_subscriber *next_sub = get_next_subscriber(broker, subscriber, msg_id);
	while (next_sub != NULL) {

		if ((current_work_q == next_sub->work_q) && k_fifo_is_empty(&next_sub->fifo)) {
			__ASSERT(next_sub->msg_handler != NULL, "");
			// We are already running on the next subscriber's work queue and its fifo
			// is empty so we can immediately call the subscriber's message handler
			next_sub->msg_handler(next_sub, msg_id, msg);
		} else {
			// Either we aren't on the correct work_q or the subscriber already has
			// messages queued so we need to queue this message with the subscriber so
			// it can handle it later
			pub_sub_acquire_msg(msg);
			pub_sub_msg_fifo_put(&next_sub->fifo, msg);
			k_work_submit_to_queue(next_sub->work_q, &next_sub->work);
			// A message can only be queued on one fifo at a time so break out of the
			// loop, subsequent subscribers will be processed in next_sub's work handler
			break;
		}
		next_sub = get_next_subscriber(broker, next_sub, msg_id);
	}
}

static struct pub_sub_subscriber *get_next_subscriber(struct pub_sub_broker *broker,
						      struct pub_sub_subscriber *subscriber,
						      uint16_t msg_id)
{
	k_mutex_lock(&broker->sub_list_mutex, K_FOREVER);
	struct pub_sub_subscriber *next_sub =
		subscriber != NULL ? SYS_SLIST_PEEK_NEXT_CONTAINER(subscriber, sub_list_node)
				   : SYS_SLIST_PEEK_HEAD_CONTAINER(&broker->subscribers, next_sub,
								   sub_list_node);
	while (next_sub != NULL) {
		if ((msg_id <= next_sub->max_pub_msg_id) &&
		    atomic_test_bit(next_sub->subs_bitarray, msg_id)) {
			break;
		}
		next_sub = SYS_SLIST_PEEK_NEXT_CONTAINER(next_sub, sub_list_node);
	}
	k_mutex_unlock(&broker->sub_list_mutex);
	return next_sub;
}

#ifdef CONFIG_PUB_SUB_DEFAULT_BROKER
PUB_SUB_BROKER_DEFINE(g_pub_sub_default_broker, &k_sys_work_q);
#endif