/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <pub_sub/delayable_msg.h>

// TODO: not sure what the best way to get access to these z_*_timeout functions is...
static inline void z_init_timeout(struct _timeout *to)
{
	sys_dnode_init(&to->node);
}

extern void z_add_timeout(struct _timeout *to, _timeout_func_t fn, k_timeout_t timeout);
extern int z_abort_timeout(struct _timeout *to);

void pub_sub_delayable_msg_init(void *msg, struct pub_sub_subscriber *subscriber, uint16_t msg_id)
{
	__ASSERT(msg != NULL, "");
	struct pub_sub_msg *ps_msg = CONTAINER_OF(msg, struct pub_sub_msg, msg);
	struct pub_sub_msg_delayable *delayable_msg =
		CONTAINER_OF(ps_msg, struct pub_sub_msg_delayable, pub_sub_msg);
	z_init_timeout(&delayable_msg->timeout);
	delayable_msg->subscriber = subscriber;
	pub_sub_msg_init(msg, msg_id, PUB_SUB_ALLOC_ID_STATIC_MSG);
}

void pub_sub_delayable_msg_start(const void *msg, k_timeout_t delay)
{
	__ASSERT(msg != NULL, "");
	struct pub_sub_msg *ps_msg = CONTAINER_OF(msg, struct pub_sub_msg, msg);
	struct pub_sub_msg_delayable *delayable_msg =
		CONTAINER_OF(ps_msg, struct pub_sub_msg_delayable, pub_sub_msg);
	z_add_timeout(&delayable_msg->timeout, pub_sub_delayable_msg_handler, delay);
}

int pub_sub_delayable_msg_update_timeout(const void *msg, k_timeout_t delay)
{
	__ASSERT(msg != NULL, "");
	int ret = pub_sub_delayable_msg_abort(msg);
	pub_sub_delayable_msg_start(msg, delay);
	return ret;
}

int pub_sub_delayable_msg_abort(const void *msg)
{
	__ASSERT(msg != NULL, "");
	struct pub_sub_msg *ps_msg = CONTAINER_OF(msg, struct pub_sub_msg, msg);
	struct pub_sub_msg_delayable *delayable_msg =
		CONTAINER_OF(ps_msg, struct pub_sub_msg_delayable, pub_sub_msg);
	int ret = z_abort_timeout(&delayable_msg->timeout);
	// If the timer abort failed check the message's reference count to see if it is in use
	if ((ret != 0) && (pub_sub_msg_get_ref_cnt(msg) == 0)) {
		ret = 0;
	}
	return ret;
}

void pub_sub_delayable_msg_handler(struct _timeout *t)
{
	struct pub_sub_msg_delayable *delayable_msg =
		CONTAINER_OF(t, struct pub_sub_msg_delayable, timeout);
	void *msg = delayable_msg->pub_sub_msg.msg;
	// Ref count must be zero otherwise the message is already queued i.e. it has been published
	// without being handled yet.
	__ASSERT(pub_sub_msg_get_ref_cnt(msg) == 0, "");
	pub_sub_acquire_msg(msg);
	pub_sub_publish_to_subscriber(delayable_msg->subscriber, msg);
}