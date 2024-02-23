/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <pub_sub/pub_sub.h>
#include <zephyr/init.h>

static void publish_work_handler(struct k_work *work);
static void process_msg(struct pub_sub_broker *broker, uint16_t msg_id, void *msg);

void pub_sub_init_broker(struct pub_sub_broker *broker)
{
	__ASSERT(broker != NULL, "");
	k_fifo_init(&broker->msg_publish_fifo);
	k_work_poll_init(&broker->publish_work, publish_work_handler);
	k_mutex_init(&broker->sub_list_mutex);
	sys_slist_init(&broker->subscribers);
	k_poll_event_init(&broker->publish_work_poll_event, K_POLL_TYPE_FIFO_DATA_AVAILABLE,
			  K_POLL_MODE_NOTIFY_ONLY, &broker->msg_publish_fifo);
	k_work_poll_submit(&broker->publish_work, &broker->publish_work_poll_event, 1, K_FOREVER);
}

void pub_sub_add_subscriber_to_broker(struct pub_sub_broker *broker,
				      struct pub_sub_subscriber *subscriber)
{
	__ASSERT(broker != NULL, "");
	__ASSERT(subscriber != NULL, "");
	__ASSERT(subscriber->handler_data.msg_handler != NULL, "");
	__ASSERT(subscriber->broker == NULL, "");
	sys_snode_t *prev_node = NULL;
	struct pub_sub_subscriber *current = NULL;
	subscriber->broker = broker;
	// Subscribers get sorted by type first: callbacks, msgq and then fifo.
	// Then they are sorted by priority value for each type
	k_mutex_lock(&broker->sub_list_mutex, K_FOREVER);
	// Search for the start of our rx_type
	current = SYS_SLIST_PEEK_HEAD_CONTAINER(&broker->subscribers, current, sub_list_node);
	while (current != NULL) {
		if (current->rx_type == subscriber->rx_type) {
			break;
		}
		prev_node = &current->sub_list_node;
		current = SYS_SLIST_PEEK_NEXT_CONTAINER(current, sub_list_node);
	}
	// Then iterate until we find a node with a larger priority value than the new one or we run
	// into the next rx_type
	while (current != NULL) {
		if ((current->priority > subscriber->priority) ||
		    (current->rx_type != subscriber->rx_type)) {
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

static void publish_work_handler(struct k_work *work)
{
	struct pub_sub_broker *broker = CONTAINER_OF(CONTAINER_OF(work, struct k_work_poll, work),
						     struct pub_sub_broker, publish_work);

	void *msg = pub_sub_msg_fifo_get(&broker->msg_publish_fifo, K_NO_WAIT);
	while (msg != NULL) {
		uint16_t msg_id = pub_sub_msg_get_msg_id(msg);
		process_msg(broker, msg_id, msg);
		msg = pub_sub_msg_fifo_get(&broker->msg_publish_fifo, K_NO_WAIT);
	}
	broker->publish_work_poll_event.state = K_POLL_STATE_NOT_READY;
	k_work_poll_submit(&broker->publish_work, &broker->publish_work_poll_event, 1, K_FOREVER);
}

static void process_msg(struct pub_sub_broker *broker, uint16_t msg_id, void *msg)
{
	bool fifo_sub_handled = false;
	struct pub_sub_subscriber *sub, *tmp;
	k_mutex_lock(&broker->sub_list_mutex, K_FOREVER);
	SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&broker->subscribers, sub, tmp, sub_list_node) {
		if ((msg_id <= sub->max_pub_msg_id) &&
		    atomic_test_bit(sub->subs_bitarray, msg_id)) {
			switch (sub->rx_type) {
			case PUB_SUB_RX_TYPE_CALLBACK: {
				__ASSERT(sub->handler_data.msg_handler != NULL, "");
				sub->handler_data.msg_handler(msg_id, msg,
							      sub->handler_data.user_data);
				break;
			}
			case PUB_SUB_RX_TYPE_MSGQ: {
				pub_sub_acquire_msg(msg);
				k_msgq_put(sub->msgq, &msg, K_FOREVER);
				break;
			}
			case PUB_SUB_RX_TYPE_FIFO: {
				if (!fifo_sub_handled) {
					pub_sub_acquire_msg(msg);
					pub_sub_msg_fifo_put(&sub->fifo, msg);
					fifo_sub_handled = true;
				}
				break;
			}
			}
			// A message can only be queued on a single fifo subscriber at a time and
			// fifo subscribers are all at the end of the list. So if the message has
			// been queued for a fifo subscriber we can just break out of the loop.
			if (fifo_sub_handled) {
				break;
			}
		}
	}
	k_mutex_unlock(&broker->sub_list_mutex);
	pub_sub_release_msg(msg);
}

#ifdef CONFIG_PUB_SUB_DEFAULT_BROKER
struct pub_sub_broker g_pub_sub_default_broker;

static int pub_sub_init_default_broker(void)
{
	pub_sub_init_broker(&g_pub_sub_default_broker);
	return 0;
}

SYS_INIT(pub_sub_init_default_broker, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
#endif