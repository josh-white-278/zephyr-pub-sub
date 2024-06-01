/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <pub_sub/pub_sub.h>
#include <pub_sub/msg_alloc_mem_slab.h>
#include <zephyr/ztest.h>
#include <stdlib.h>

#define NUM_SUBSCRIBERS 3

enum msg_id {
	MSG_ID_SUBSCRIBED_ID_0,
	MSG_ID_NOT_SUBSCRIBED_ID_0,
	MSG_ID_SUBSCRIBED_ID_1,
	MSG_ID_NOT_SUBSCRIBED_ID_1,
	MSG_ID_SUBSCRIBED_ID_2,
	MSG_ID_NOT_SUBSCRIBED_ID_2,
	// Add a gap so we can test the sizing of the subs bitarray
	MSG_ID_GAP = ATOMIC_BITS,
	MSG_ID_ALL_SUBSCRIBED,
	MSG_ID_MAX_PUB_ID = MSG_ID_ALL_SUBSCRIBED,
};

struct rx_msg {
	uint16_t msg_id;
	struct pub_sub_subscriber *subscriber;
};

struct test_subscriber {
	PUB_SUB_SUBSCRIBER_COMPOSE(MSG_ID_MAX_PUB_ID);
	struct k_msgq *rx_msgq;
};

static void test_msg_handler(struct pub_sub_subscriber *subscriber, uint16_t msg_id,
			     const void *msg);

K_MSGQ_DEFINE(g_rx_msg_queue, sizeof(struct rx_msg), 32, 1);
PUB_SUB_MEM_SLAB_ALLOCATOR_DEFINE_STATIC(test_allocator, 0, 32);

static struct test_subscriber callback_subscriber_0 = {
	PUB_SUB_SUBSCRIBER_INIT_COMPOSED(callback_subscriber_0, test_msg_handler, MSG_ID_MAX_PUB_ID,
					 PUB_SUB_RX_TYPE_CALLBACK, 0),
	.rx_msgq = &g_rx_msg_queue,
};
static struct test_subscriber callback_subscriber_1 = {
	PUB_SUB_SUBSCRIBER_INIT_COMPOSED(callback_subscriber_1, test_msg_handler, MSG_ID_MAX_PUB_ID,
					 PUB_SUB_RX_TYPE_CALLBACK, 1),
	.rx_msgq = &g_rx_msg_queue,
};
// We will use the run time init function to initialize this one's subscriber
static struct test_subscriber callback_subscriber_2 = {
	.rx_msgq = &g_rx_msg_queue,
};

static struct test_subscriber fifo_subscriber_0 = {
	PUB_SUB_SUBSCRIBER_INIT_COMPOSED(fifo_subscriber_0, test_msg_handler, MSG_ID_MAX_PUB_ID,
					 PUB_SUB_RX_TYPE_FIFO, 0),
	.rx_msgq = &g_rx_msg_queue,
};
static struct test_subscriber fifo_subscriber_1 = {
	PUB_SUB_SUBSCRIBER_INIT_COMPOSED(fifo_subscriber_1, test_msg_handler, MSG_ID_MAX_PUB_ID,
					 PUB_SUB_RX_TYPE_FIFO, 1),
	.rx_msgq = &g_rx_msg_queue,
};
// We will use the run time init function to initialize this one's subscriber
static struct test_subscriber fifo_subscriber_2 = {
	.rx_msgq = &g_rx_msg_queue,
};

static struct pub_sub_subscriber *callback_subscribers[NUM_SUBSCRIBERS] = {
	PUB_SUB_COMPOSED_SUBSCRIBER_PTR(callback_subscriber_0),
	PUB_SUB_COMPOSED_SUBSCRIBER_PTR(callback_subscriber_1),
	PUB_SUB_COMPOSED_SUBSCRIBER_PTR(callback_subscriber_2),
};
static struct pub_sub_subscriber *fifo_subscribers[NUM_SUBSCRIBERS] = {
	PUB_SUB_COMPOSED_SUBSCRIBER_PTR(fifo_subscriber_0),
	PUB_SUB_COMPOSED_SUBSCRIBER_PTR(fifo_subscriber_1),
	PUB_SUB_COMPOSED_SUBSCRIBER_PTR(fifo_subscriber_2),
};

static void test_msg_handler(struct pub_sub_subscriber *subscriber, uint16_t msg_id,
			     const void *msg)
{
	struct test_subscriber *test_subscriber =
		PUB_SUB_CONTAINER_FROM_SUBSCRIBER(subscriber, struct test_subscriber);
	struct rx_msg rx_msg = {
		.msg_id = msg_id,
		.subscriber = subscriber,
	};
	k_msgq_put(test_subscriber->rx_msgq, &rx_msg, K_FOREVER);
}

static void *suite_setup(void)
{
	// Initialize the two uninitialized subscribers with the run time initialization function
	pub_sub_init_subscriber(&callback_subscriber_2._subscriber, test_msg_handler,
				callback_subscriber_2._subs_bitarray, MSG_ID_MAX_PUB_ID,
				PUB_SUB_RX_TYPE_CALLBACK, 2);
	pub_sub_init_subscriber(&fifo_subscriber_2._subscriber, test_msg_handler,
				fifo_subscriber_2._subs_bitarray, MSG_ID_MAX_PUB_ID,
				PUB_SUB_RX_TYPE_FIFO, 2);

	for (size_t i = 0; i < NUM_SUBSCRIBERS; i++) {
		pub_sub_add_subscriber(callback_subscribers[i]);
		pub_sub_add_subscriber(fifo_subscribers[i]);
	}
	return NULL;
}

static void before_test(void *fixture)
{
	ARG_UNUSED(fixture);
	for (size_t i = 0; i < NUM_SUBSCRIBERS; i++) {
		pub_sub_subscribe(callback_subscribers[i], MSG_ID_SUBSCRIBED_ID_0 + i * 2);
		pub_sub_subscribe(callback_subscribers[i], MSG_ID_ALL_SUBSCRIBED);
		pub_sub_subscribe(fifo_subscribers[i], MSG_ID_SUBSCRIBED_ID_0 + i * 2);
		pub_sub_subscribe(fifo_subscribers[i], MSG_ID_ALL_SUBSCRIBED);
	}
	k_msgq_purge(&g_rx_msg_queue);
}

static void after_test(void *fixture)
{
	ARG_UNUSED(fixture);
	// Check for leaked messages
	struct k_mem_slab *mem_slab = test_allocator.impl;
	__ASSERT(k_mem_slab_num_used_get(mem_slab) == 0, "");
}

ZTEST(subscriber, test_subscription)
{
	struct rx_msg rx_msg;
	void *msg;

	for (size_t i = 0; i < NUM_SUBSCRIBERS; i++) {
		msg = pub_sub_new_msg(&test_allocator, MSG_ID_SUBSCRIBED_ID_0 + 2 * i, 0,
				      K_NO_WAIT);
		zassert_not_null(msg);
		pub_sub_publish(msg);

		zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(10)));
		zassert_equal(rx_msg.msg_id, MSG_ID_SUBSCRIBED_ID_0 + 2 * i);
		zassert_equal_ptr(rx_msg.subscriber, callback_subscribers[i]);

		zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(10)));
		zassert_equal(rx_msg.msg_id, MSG_ID_SUBSCRIBED_ID_0 + 2 * i);
		zassert_equal_ptr(rx_msg.subscriber, fifo_subscribers[i]);
	}

	msg = pub_sub_new_msg(&test_allocator, MSG_ID_NOT_SUBSCRIBED_ID_0, 0, K_NO_WAIT);
	zassert_not_null(msg);
	pub_sub_publish(msg);
	msg = pub_sub_new_msg(&test_allocator, MSG_ID_NOT_SUBSCRIBED_ID_1, 0, K_NO_WAIT);
	zassert_not_null(msg);
	pub_sub_publish(msg);
	msg = pub_sub_new_msg(&test_allocator, MSG_ID_NOT_SUBSCRIBED_ID_2, 0, K_NO_WAIT);
	zassert_not_null(msg);
	pub_sub_publish(msg);

	zassert_not_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(10)));
}

ZTEST(subscriber, test_unsubscribe)
{
	struct rx_msg rx_msg;
	void *msg;

	pub_sub_unsubscribe(fifo_subscribers[0], MSG_ID_SUBSCRIBED_ID_0);

	msg = pub_sub_new_msg(&test_allocator, MSG_ID_SUBSCRIBED_ID_0, 0, K_NO_WAIT);
	zassert_not_null(msg);
	pub_sub_publish(msg);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(10)));
	zassert_equal(rx_msg.msg_id, MSG_ID_SUBSCRIBED_ID_0);
	zassert_equal_ptr(rx_msg.subscriber, callback_subscribers[0]);

	zassert_not_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(10)));
}

ZTEST(subscriber, test_priority)
{
	struct rx_msg rx_msg;
	// Remove all of the subscribers so we can test priority sorting
	for (size_t i = 0; i < NUM_SUBSCRIBERS; i++) {
		pub_sub_subscriber_remove_broker(callback_subscribers[i]);
		pub_sub_subscriber_remove_broker(fifo_subscribers[i]);
	}
	// Add the subscribers and test that they are sorted by type and priority, where priority is
	// set to the index of the subscriber in its array above
	pub_sub_add_subscriber(fifo_subscribers[1]);
	pub_sub_add_subscriber(callback_subscribers[1]);
	pub_sub_add_subscriber(fifo_subscribers[2]);
	pub_sub_add_subscriber(callback_subscribers[2]);
	pub_sub_add_subscriber(fifo_subscribers[0]);
	pub_sub_add_subscriber(callback_subscribers[0]);

	void *msg = pub_sub_new_msg(&test_allocator, MSG_ID_ALL_SUBSCRIBED, 0, K_NO_WAIT);
	zassert_not_null(msg);
	pub_sub_publish(msg);
	for (size_t i = 0; i < NUM_SUBSCRIBERS; i++) {
		zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(10)));
		zassert_equal(rx_msg.msg_id, MSG_ID_ALL_SUBSCRIBED);
		zassert_equal_ptr(rx_msg.subscriber, callback_subscribers[i]);
	}
	for (size_t i = 0; i < NUM_SUBSCRIBERS; i++) {
		zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(10)));
		zassert_equal(rx_msg.msg_id, MSG_ID_ALL_SUBSCRIBED);
		zassert_equal_ptr(rx_msg.subscriber, fifo_subscribers[i]);
	}
}

ZTEST(subscriber, test_publish_to_subscriber)
{
	struct rx_msg rx_msg;
	uint16_t priv_id = MSG_ID_MAX_PUB_ID + 1;

	void *msg = pub_sub_new_msg(&test_allocator, priv_id, 0, K_NO_WAIT);
	zassert_not_null(msg);
	pub_sub_publish_to_subscriber(callback_subscribers[1], msg);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(10)));
	zassert_equal(rx_msg.msg_id, priv_id);
	zassert_equal_ptr(rx_msg.subscriber, callback_subscribers[1]);

	msg = pub_sub_new_msg(&test_allocator, priv_id, 0, K_NO_WAIT);
	zassert_not_null(msg);
	pub_sub_publish_to_subscriber(fifo_subscribers[1], msg);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(10)));
	zassert_equal(rx_msg.msg_id, priv_id);
	zassert_equal_ptr(rx_msg.subscriber, fifo_subscribers[1]);
}

ZTEST_SUITE(subscriber, NULL, suite_setup, before_test, after_test, NULL);

void fifo_thread_handler(void *unused0, void *unused1, void *unused2)
{
	while (1) {
		k_sleep(K_MSEC(1));
		for (size_t i = 0; i < ARRAY_SIZE(fifo_subscribers); i++) {
			pub_sub_handle_queued_msg(fifo_subscribers[i], K_NO_WAIT);
		}
	}
}
K_THREAD_DEFINE(fifo_thread, 4096, fifo_thread_handler, NULL, NULL, NULL, 0, 0, 0);