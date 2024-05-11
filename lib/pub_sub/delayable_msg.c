/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <pub_sub/delayable_msg.h>

#define RETRY_TIMEOUT K_MSEC(CONFIG_PUB_SUB_EXPIRED_DELAYABLE_MSG_RETRY_TIME_MS)

static void add_to_wait_list(struct pub_sub_msg_delayable *msg_to_add,
			     struct pub_sub_msg_delayable *next_msg);
static void timer_timeout(struct k_timer *timer);

static sys_dlist_t g_wait_list = SYS_DLIST_STATIC_INIT(&g_wait_list);
static sys_dlist_t g_expired_list = SYS_DLIST_STATIC_INIT(&g_expired_list);
static K_TIMER_DEFINE(g_msg_timer, timer_timeout, NULL);
static struct k_spinlock g_spin_lock;
static k_timepoint_t g_current_end_time;

void pub_sub_delayable_msg_init(void *msg, struct pub_sub_subscriber *subscriber, uint16_t msg_id)
{
	__ASSERT(msg != NULL, "");
	struct pub_sub_msg_delayable *delayable_msg =
		CONTAINER_OF(msg, struct pub_sub_msg_delayable, msg);
	sys_dnode_init(&delayable_msg->header.node);
	delayable_msg->header.subscriber = subscriber;
	pub_sub_msg_init(msg, msg_id, PUB_SUB_ALLOC_ID_STATIC_MSG);
}

int pub_sub_delayable_msg_start(const void *msg, k_timeout_t timeout)
{
	__ASSERT(msg != NULL, "");
	struct pub_sub_msg_delayable *delayable_msg =
		CONTAINER_OF(msg, struct pub_sub_msg_delayable, msg);
	int ret = -EBUSY;
	K_SPINLOCK(&g_spin_lock)
	{
		k_timepoint_t new_end_time = sys_timepoint_calc(timeout);
		// Assume we are going to insert at the head of the list
		struct pub_sub_msg_delayable *insert_msg =
			SYS_DLIST_PEEK_HEAD_CONTAINER(&g_wait_list, insert_msg, header.node);
		ret = (pub_sub_msg_get_ref_cnt(msg) == 0) ? 0 : -EBUSY;
		// If the message is linked then we need to remove it
		if (sys_dnode_is_linked(&delayable_msg->header.node)) {
			// If the new end time is further away than the current end time then move
			// the insert_msg to the next message in the queue rather than the head
			// message. Make sure the current end time isn't expired though, otherwise
			// we will be peeking into the expired list rather than the wait list
			if (!sys_timepoint_expired(delayable_msg->header.end_time) &&
			    (sys_timepoint_cmp(new_end_time, delayable_msg->header.end_time) > 0)) {
				insert_msg = SYS_DLIST_PEEK_NEXT_CONTAINER(
					&g_wait_list, delayable_msg, header.node);
				sys_dlist_remove(&delayable_msg->header.node);
			} else {
				sys_dlist_remove(&delayable_msg->header.node);
				// Update the head of the list pointer in case it was the message
				// being updated
				insert_msg = SYS_DLIST_PEEK_HEAD_CONTAINER(&g_wait_list, insert_msg,
									   header.node);
			}
		}
		delayable_msg->header.end_time = new_end_time;
		add_to_wait_list(delayable_msg, insert_msg);
		// Update the timer timeout if needed, we can't update if there are expired messages
		// because we don't know if the current timeout came from the expired retry or the
		// wait list
		if (sys_dlist_is_empty(&g_expired_list)) {
			struct pub_sub_msg_delayable *head_msg = SYS_DLIST_PEEK_HEAD_CONTAINER(
				&g_wait_list, insert_msg, header.node);
			// head_msg can't be null because we just added a message.
			// Update the msg_timer timeout if the updated message is the at the head of
			// the wait list or the current end time doesn't equal the head message's
			// end time
			if ((head_msg == delayable_msg) ||
			    (sys_timepoint_cmp(head_msg->header.end_time, g_current_end_time) !=
			     0)) {
				g_current_end_time = head_msg->header.end_time;
				k_timer_start(&g_msg_timer,
					      sys_timepoint_timeout(head_msg->header.end_time),
					      K_NO_WAIT);
			}
		}
	}
	return ret;
}

int pub_sub_delayable_msg_abort(const void *msg)
{
	__ASSERT(msg != NULL, "");
	struct pub_sub_msg_delayable *delayable_msg =
		CONTAINER_OF(msg, struct pub_sub_msg_delayable, msg);
	int ret = -EBUSY;
	K_SPINLOCK(&g_spin_lock)
	{
		ret = (pub_sub_msg_get_ref_cnt(msg) == 0) ? 0 : -EBUSY;
		if (sys_dnode_is_linked(&delayable_msg->header.node)) {
			sys_dlist_remove(&delayable_msg->header.node);
			struct pub_sub_msg_delayable *head_msg =
				SYS_DLIST_PEEK_HEAD_CONTAINER(&g_wait_list, head_msg, header.node);
			bool expired_list_empty = sys_dlist_is_empty(&g_expired_list);
			if ((head_msg == NULL) && expired_list_empty) {
				// Both lists empty stop the timer
				k_timer_stop(&g_msg_timer);
			} else if ((expired_list_empty) &&
				   (sys_timepoint_cmp(head_msg->header.end_time,
						      g_current_end_time) > 0)) {
				// If expired list is empty and the head message in the wait list is
				// further away than the current timeout then update the timeout to
				// the head message's timeout
				g_current_end_time = head_msg->header.end_time;
				k_timer_start(&g_msg_timer,
					      sys_timepoint_timeout(head_msg->header.end_time),
					      K_NO_WAIT);
			}
		}
	}
	return ret;
}

bool pub_sub_delayable_msg_is_active(const void *msg)
{
	const struct pub_sub_msg_delayable *delayable_msg =
		CONTAINER_OF(msg, struct pub_sub_msg_delayable, msg);
	bool ret = false;
	K_SPINLOCK(&g_spin_lock)
	{
		ret = sys_dnode_is_linked(&delayable_msg->header.node);
	}
	return ret;
}

// Must be called with the g_spin_lock held
static void add_to_wait_list(struct pub_sub_msg_delayable *msg_to_add,
			     struct pub_sub_msg_delayable *next_msg)
{
	// Search through the wait list from next_msg until we find a message with an end_time that
	// is further away. Then insert the msg_to_add before next_msg so that wait_list is always
	// sorted by message end_time.
	while ((next_msg != NULL) &&
	       (sys_timepoint_cmp(next_msg->header.end_time, msg_to_add->header.end_time) <= 0)) {
		next_msg = SYS_DLIST_PEEK_NEXT_CONTAINER(&g_wait_list, next_msg, header.node);
	}
	if (next_msg == NULL) {
		sys_dlist_append(&g_wait_list, &msg_to_add->header.node);
	} else {
		sys_dlist_insert(&next_msg->header.node, &msg_to_add->header.node);
	}
}

static void timer_timeout(struct k_timer *timer)
{
	K_SPINLOCK(&g_spin_lock)
	{
		// Handle any messages in the expired list first
		struct pub_sub_msg_delayable *delayable_msg =
			SYS_DLIST_PEEK_HEAD_CONTAINER(&g_expired_list, delayable_msg, header.node);
		while (delayable_msg != NULL) {
			void *msg = delayable_msg->msg;
			struct pub_sub_msg_delayable *next_msg = SYS_DLIST_PEEK_NEXT_CONTAINER(
				&g_expired_list, delayable_msg, header.node);
			if (pub_sub_msg_get_ref_cnt(msg) == 0) {
				pub_sub_acquire_msg(msg);
				pub_sub_publish_to_subscriber(delayable_msg->header.subscriber,
							      msg);
				sys_dlist_remove(&delayable_msg->header.node);
			}
			delayable_msg = next_msg;
		}
		// Publish all expired messages in the wait list
		delayable_msg =
			SYS_DLIST_PEEK_HEAD_CONTAINER(&g_wait_list, delayable_msg, header.node);
		while ((delayable_msg != NULL) &&
		       sys_timepoint_expired(delayable_msg->header.end_time)) {
			struct pub_sub_msg_delayable *tmp_msg = delayable_msg;
			void *msg = tmp_msg->msg;
			delayable_msg =
				SYS_DLIST_PEEK_NEXT_CONTAINER(&g_wait_list, tmp_msg, header.node);
			sys_dlist_remove(&tmp_msg->header.node);
			if (pub_sub_msg_get_ref_cnt(msg) == 0) {
				pub_sub_acquire_msg(msg);
				pub_sub_publish_to_subscriber(tmp_msg->header.subscriber, msg);
			} else {
				sys_dlist_append(&g_expired_list, &tmp_msg->header.node);
			}
		}
		// Find the next timeout, delayable_msg is pointing to the head of wait_list.
		k_timeout_t new_timeout = K_FOREVER;
		if (delayable_msg != NULL) {
			new_timeout = sys_timepoint_timeout(delayable_msg->header.end_time);
			g_current_end_time = delayable_msg->header.end_time;
		}
		if (!sys_dlist_is_empty(&g_expired_list)) {
			k_timepoint_t retry_timepoint = sys_timepoint_calc(RETRY_TIMEOUT);
			if (K_TIMEOUT_EQ(new_timeout, K_FOREVER) ||
			    (sys_timepoint_cmp(retry_timepoint, g_current_end_time) < 0)) {
				new_timeout = RETRY_TIMEOUT;
				g_current_end_time = retry_timepoint;
			}
		}
		if (!K_TIMEOUT_EQ(new_timeout, K_FOREVER)) {
			k_timer_start(timer, new_timeout, K_NO_WAIT);
		}
	}
}