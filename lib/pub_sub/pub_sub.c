/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <pub_sub/pub_sub.h>
#include <zephyr/init.h>
#include <string.h>

// Define the default broker if it is enabled
#ifdef CONFIG_PUB_SUB_DEFAULT_BROKER
PUB_SUB_BROKER_DEFINE(g_pub_sub_default_broker, &k_sys_work_q);
#endif

static void process_public_msg(struct pub_sub_subscriber *subscriber, uint16_t msg_id, void *msg,
			       struct k_work_q *current_work_q);
static struct pub_sub_subscriber *
get_next_subscriber_for_msg_id_from_subscriber(struct pub_sub_subscriber *subscriber,
					       uint16_t msg_id);
static struct pub_sub_subscriber *
get_next_subscriber_for_msg_id_from_broker(struct pub_sub_broker *broker, uint16_t msg_id);

void pub_sub_broker_work_handler(struct k_work *work)
{
	struct pub_sub_broker *broker = CONTAINER_OF(work, struct pub_sub_broker, work);
	void *msg = pub_sub_msg_fifo_get(&broker->msg_fifo, K_NO_WAIT);
	while (msg != NULL) {
		uint16_t msg_id = pub_sub_msg_get_msg_id(msg);
		struct pub_sub_subscriber *next_subscriber =
			get_next_subscriber_for_msg_id_from_broker(broker, msg_id);
		process_public_msg(next_subscriber, msg_id, msg, broker->work_q);
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
			struct pub_sub_subscriber *next_subscriber =
				get_next_subscriber_for_msg_id_from_subscriber(subscriber, msg_id);
			process_public_msg(next_subscriber, msg_id, msg, subscriber->work_q);
		}
		pub_sub_release_msg(msg);
		msg = pub_sub_msg_fifo_get(&subscriber->fifo, K_NO_WAIT);
	}
}

static void process_public_msg(struct pub_sub_subscriber *subscriber, uint16_t msg_id, void *msg,
			       struct k_work_q *current_work_q)
{
	while (subscriber != NULL) {

		if ((current_work_q == subscriber->work_q) && k_fifo_is_empty(&subscriber->fifo)) {
			__ASSERT(subscriber->msg_handler != NULL, "");
			// We are already running on the next subscriber's work queue and its fifo
			// is empty so we can immediately call the subscriber's message handler
			subscriber->msg_handler(subscriber, msg_id, msg);
		} else {
			// Either we aren't on the correct work_q or the subscriber already has
			// messages queued so we need to queue this message with the subscriber so
			// it can handle it later
			pub_sub_acquire_msg(msg);
			pub_sub_msg_fifo_put(&subscriber->fifo, msg);
			k_work_submit_to_queue(subscriber->work_q, &subscriber->work);
			// A message can only be queued on one fifo at a time so break out of the
			// loop, subsequent subscribers will be processed in the subscriber's work
			// handler
			break;
		}
		subscriber = get_next_subscriber_for_msg_id_from_subscriber(subscriber, msg_id);
	}
}

#if defined(CONFIG_PUB_SUB_RUNTIME_SUBSCRIBERS)
static struct pub_sub_subscriber *
get_next_subscriber_for_msg_id_from_subscriber(struct pub_sub_subscriber *subscriber,
					       uint16_t msg_id)
{
	k_mutex_lock(&subscriber->broker->sub_list_mutex, K_FOREVER);
	struct pub_sub_subscriber *next_sub =
		SYS_SLIST_PEEK_NEXT_CONTAINER(subscriber, sub_list_node);
	while (next_sub != NULL) {
		if ((msg_id <= next_sub->max_pub_msg_id) &&
		    atomic_test_bit(next_sub->subs_bitarray, msg_id)) {
			break;
		}
		next_sub = SYS_SLIST_PEEK_NEXT_CONTAINER(next_sub, sub_list_node);
	}
	k_mutex_unlock(&subscriber->broker->sub_list_mutex);
	return next_sub;
}

static struct pub_sub_subscriber *
get_next_subscriber_for_msg_id_from_broker(struct pub_sub_broker *broker, uint16_t msg_id)
{
	k_mutex_lock(&broker->sub_list_mutex, K_FOREVER);
	struct pub_sub_subscriber *next_sub =
		SYS_SLIST_PEEK_HEAD_CONTAINER(&broker->subscribers, next_sub, sub_list_node);
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

#else
static struct pub_sub_subscriber *
get_next_subscriber_for_msg_id_from_entry(const struct pub_sub_broker_subscriber_entry *start_entry,
					  uint16_t msg_id)
{
	STRUCT_SECTION_END_EXTERN(pub_sub_broker_subscriber_entry);
	const struct pub_sub_broker_subscriber_entry *subscriber_entry_section_end =
		STRUCT_SECTION_END(pub_sub_broker_subscriber_entry);
	const struct pub_sub_broker_subscriber_entry *next_entry = start_entry + 1;
	struct pub_sub_subscriber *next_sub = NULL;
	// next_entry->subscriber == NULL is the next broker's subscriber entry so iterate until end
	// of section or NULL subscriber
	while ((next_entry < subscriber_entry_section_end) && (next_entry->subscriber != NULL)) {
		if ((msg_id <= next_entry->subscriber->max_pub_msg_id) &&
		    atomic_test_bit(next_entry->subscriber->subs_bitarray, msg_id)) {
			next_sub = next_entry->subscriber;
			break;
		}
		next_entry = next_entry + 1;
	}
	return next_sub;
}

static struct pub_sub_subscriber *
get_next_subscriber_for_msg_id_from_subscriber(struct pub_sub_subscriber *subscriber,
					       uint16_t msg_id)
{
	return get_next_subscriber_for_msg_id_from_entry(subscriber->broker_entry, msg_id);
}

static struct pub_sub_subscriber *
get_next_subscriber_for_msg_id_from_broker(struct pub_sub_broker *broker, uint16_t msg_id)
{
	return get_next_subscriber_for_msg_id_from_entry(broker->subscribers_start, msg_id);
}

// When runtime subscribers are not enabled each subscriber needs to have its broker_entry pointer
// set to its entry so that it is possible to iterate from one subscriber to the next. This could be
// done statically but it would need to use tentative definitions or something as there is a
// circular dependency between the definitions of the subscriber entry and the subscriber. The
// additional complexity didn't seem worth it so initialize the broker_entry pointers at runtime.
static int pub_sub_init_static_subscribers(void)
{
	STRUCT_SECTION_FOREACH(pub_sub_broker_subscriber_entry, sub_entry) {
		if (sub_entry->subscriber != NULL) {
			sub_entry->subscriber->broker_entry = sub_entry;
		}
	}
	return 0;
}
SYS_INIT(pub_sub_init_static_subscribers, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
#endif // CONFIG_PUB_SUB_RUNTIME_SUBSCRIBERS
