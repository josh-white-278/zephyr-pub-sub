/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <pub_sub/runtime_subscriber.h>
#include <pub_sub/pub_sub.h>
#include <string.h>
#include <zephyr/init.h>

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
			     uint16_t max_pub_msg_id)
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
	subscriber->priority = 0;
}

void pub_sub_add_subscriber_to_broker(struct pub_sub_broker *broker,
				      struct pub_sub_subscriber *subscriber, uint8_t priority)
{
	__ASSERT(broker != NULL, "");
	__ASSERT(subscriber != NULL, "");
	__ASSERT(subscriber->msg_handler != NULL, "");
	__ASSERT(subscriber->broker == NULL, "");
	sys_snode_t *prev_node = NULL;
	struct pub_sub_subscriber *current = NULL;
	subscriber->broker = broker;
	subscriber->priority = priority;
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

// When runtime subscribers are enabled the iterable section subscribers need to be added
// into the linked list so that there is a single list of prioritized subscribers
static int pub_sub_init_runtime_subscribers(void)
{
	STRUCT_SECTION_END_EXTERN(pub_sub_broker_subscriber_entry);
	const struct pub_sub_broker_subscriber_entry *section_end =
		STRUCT_SECTION_END(pub_sub_broker_subscriber_entry);
	// Every broker's subscriber section starts with a NULL subscriber so we iterate from (start
	// + 1) to the next NULL subscriber which is the start of the next broker
	STRUCT_SECTION_FOREACH(pub_sub_broker, broker) {
		const struct pub_sub_broker_subscriber_entry *broker_entry =
			broker->subscribers_start;
		if (broker_entry != NULL) {
			__ASSERT(broker_entry->subscriber == NULL,
				 "Broker start of subscribers should start with a NULL subscriber");

			broker_entry++;
			while ((broker_entry->subscriber != NULL) && (broker_entry < section_end)) {
				pub_sub_add_subscriber_to_broker(broker, broker_entry->subscriber,
								 broker_entry->priority);
				broker_entry++;
			}
		}
	}
	return 0;
}
SYS_INIT(pub_sub_init_runtime_subscribers, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);