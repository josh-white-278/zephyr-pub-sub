/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <pub_sub/delayable_msg.h>

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
	delayable_msg->header.aborted = false;
	pub_sub_msg_init(msg, msg_id, PUB_SUB_ALLOC_ID_DELAYABLE_MSG);
}

void pub_sub_delayable_msg_start(const void *msg, k_timeout_t timeout)
{
	__ASSERT(msg != NULL, "");
	struct pub_sub_msg_delayable *delayable_msg =
		CONTAINER_OF(msg, struct pub_sub_msg_delayable, msg);
	delayable_msg->header.aborted = pub_sub_msg_get_ref_cnt(msg) != 0;
	k_timepoint_t new_end_time = sys_timepoint_calc(timeout);
	K_SPINLOCK(&g_spin_lock)
	{
		// Assume we are going to insert at the head of the list
		struct pub_sub_msg_delayable *insert_msg =
			SYS_DLIST_PEEK_HEAD_CONTAINER(&g_wait_list, insert_msg, header.node);
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
		// Update the timer timeout if needed
		struct pub_sub_msg_delayable *head_msg =
			SYS_DLIST_PEEK_HEAD_CONTAINER(&g_wait_list, insert_msg, header.node);
		// head_msg can't be null because we just added a message.
		// Update the msg_timer timeout if the updated message is the at the head of
		// the wait list or the current end time doesn't equal the head message's
		// end time
		if ((head_msg == delayable_msg) ||
		    (sys_timepoint_cmp(head_msg->header.end_time, g_current_end_time) != 0)) {
			g_current_end_time = head_msg->header.end_time;
			k_timer_start(&g_msg_timer,
				      sys_timepoint_timeout(head_msg->header.end_time), K_NO_WAIT);
		}
	}
}

void pub_sub_delayable_msg_abort(const void *msg)
{
	__ASSERT(msg != NULL, "");
	struct pub_sub_msg_delayable *delayable_msg =
		CONTAINER_OF(msg, struct pub_sub_msg_delayable, msg);
	delayable_msg->header.aborted = pub_sub_msg_get_ref_cnt(msg) != 0;
	K_SPINLOCK(&g_spin_lock)
	{
		if (sys_dnode_is_linked(&delayable_msg->header.node)) {
			sys_dlist_remove(&delayable_msg->header.node);
			struct pub_sub_msg_delayable *head_msg =
				SYS_DLIST_PEEK_HEAD_CONTAINER(&g_wait_list, head_msg, header.node);
			if (head_msg == NULL) {
				// wait list is empty stop the timer
				k_timer_stop(&g_msg_timer);
			} else if ((sys_timepoint_cmp(head_msg->header.end_time,
						      g_current_end_time) > 0)) {
				// If the head message in the wait list is further away than the
				// current timeout then update the timeout to the head message's
				// timeout
				g_current_end_time = head_msg->header.end_time;
				k_timer_start(&g_msg_timer,
					      sys_timepoint_timeout(head_msg->header.end_time),
					      K_NO_WAIT);
			}
		}
	}
}

void pub_sub_free_delayable_msg(const void *msg)
{
	__ASSERT(msg != NULL, "");
	struct pub_sub_msg_delayable *delayable_msg =
		CONTAINER_OF(msg, struct pub_sub_msg_delayable, msg);
	// Clear the aborted flag
	delayable_msg->header.aborted = false;
	// If the expired list is not empty then check if any messages can be published
	if (!sys_dlist_is_empty(&g_expired_list)) {
		K_SPINLOCK(&g_spin_lock)
		{
			delayable_msg = SYS_DLIST_PEEK_HEAD_CONTAINER(&g_expired_list,
								      delayable_msg, header.node);
			while (delayable_msg != NULL) {
				void *msg = delayable_msg->msg;
				struct pub_sub_msg_delayable *next_msg =
					SYS_DLIST_PEEK_NEXT_CONTAINER(&g_expired_list,
								      delayable_msg, header.node);
				if (pub_sub_msg_get_ref_cnt(msg) == 0) {
					sys_dlist_remove(&delayable_msg->header.node);
					pub_sub_acquire_msg(msg);
					pub_sub_publish_to_subscriber(
						delayable_msg->header.subscriber, msg);
				}
				delayable_msg = next_msg;
			}
		}
	}
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
		// Publish all expired messages in the wait list
		struct pub_sub_msg_delayable *delayable_msg =
			SYS_DLIST_PEEK_HEAD_CONTAINER(&g_wait_list, delayable_msg, header.node);
		while ((delayable_msg != NULL) &&
		       sys_timepoint_expired(delayable_msg->header.end_time)) {
			struct pub_sub_msg_delayable *tmp_msg = delayable_msg;
			void *msg = tmp_msg->msg;
			delayable_msg =
				SYS_DLIST_PEEK_NEXT_CONTAINER(&g_wait_list, tmp_msg, header.node);
			sys_dlist_remove(&tmp_msg->header.node);
			// If the reference counter is zero then the message is not queued with the
			// subscriber and we can publish it, otherwise we need to move the message
			// to the expired list so it can be handled when the message is freed.
			if (pub_sub_msg_get_ref_cnt(msg) == 0) {
				pub_sub_acquire_msg(msg);
				pub_sub_publish_to_subscriber(tmp_msg->header.subscriber, msg);
			} else {
				sys_dlist_append(&g_expired_list, &tmp_msg->header.node);
			}
		}
		// delayable_msg is pointing to the new head of wait_list and it is the next message
		// to be published so restart the timer with its timeout
		if (delayable_msg != NULL) {
			g_current_end_time = delayable_msg->header.end_time;
			k_timer_start(timer, sys_timepoint_timeout(delayable_msg->header.end_time),
				      K_NO_WAIT);
		}
	}
}