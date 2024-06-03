/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <pub_sub/pub_sub.h>
#include <pub_sub/delayable_msg.h>
#include <zephyr/ztest.h>
#include <stdlib.h>

#define assert_rx_msg(_rx_msg, _subscriber, _msg_id, _msg_ptr)                                     \
	zassert_equal(_rx_msg.subscriber, _subscriber);                                            \
	zassert_equal(_rx_msg.msg_id, _msg_id);                                                    \
	zassert_equal_ptr(_rx_msg.msg_ptr, _msg_ptr)

enum msg_id {
	MSG_ID_SUBSCRIBED_ID_0,
	MSG_ID_MAX_PUB_ID = MSG_ID_SUBSCRIBED_ID_0,
	MSG_ID_TIMER_0,
	MSG_ID_TIMER_1,
	MSG_ID_TIMER_2,
};

struct test_msg {
};

struct rx_msg {
	struct pub_sub_subscriber *subscriber;
	uint16_t msg_id;
	const void *msg_ptr;
};

struct test_subscriber {
	PUB_SUB_SUBSCRIBER_COMPOSE(MSG_ID_MAX_PUB_ID);
	struct k_msgq *rx_msgq;
};

static void msg_handler(struct pub_sub_subscriber *subscriber, uint16_t msg_id, const void *msg);

PUB_SUB_STATIC_DELAYABLE_MSG_DEFINE(struct test_msg, g_sub_0_msg_0, MSG_ID_TIMER_0, NULL);
PUB_SUB_STATIC_DELAYABLE_MSG_DEFINE(struct test_msg, g_sub_0_msg_1, MSG_ID_TIMER_1, NULL);
PUB_SUB_STATIC_DELAYABLE_MSG_DEFINE(struct test_msg, g_sub_0_msg_2, MSG_ID_TIMER_2, NULL);
PUB_SUB_STATIC_DELAYABLE_MSG_DEFINE(struct test_msg, g_sub_1_msg_0, MSG_ID_TIMER_0, NULL);
PUB_SUB_STATIC_DELAYABLE_MSG_DEFINE(struct test_msg, g_sub_1_msg_1, MSG_ID_TIMER_1, NULL);
PUB_SUB_STATIC_DELAYABLE_MSG_DEFINE(struct test_msg, g_sub_1_msg_2, MSG_ID_TIMER_2, NULL);

K_MSGQ_DEFINE(g_rx_msg_queue, sizeof(struct rx_msg), 32, 1);

static K_KERNEL_STACK_DEFINE(g_sub_0_work_q_stack, CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE);
struct k_work_q g_sub_0_work_q;

static K_KERNEL_STACK_DEFINE(g_sub_1_work_q_stack, CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE);
struct k_work_q g_sub_1_work_q;

static struct test_subscriber g_test_subscriber_0 = {
	PUB_SUB_SUBSCRIBER_INIT_COMPOSED(g_test_subscriber_0, &g_sub_0_work_q, msg_handler,
					 MSG_ID_MAX_PUB_ID, 0),
	.rx_msgq = &g_rx_msg_queue,
};
static struct test_subscriber g_test_subscriber_1 = {
	PUB_SUB_SUBSCRIBER_INIT_COMPOSED(g_test_subscriber_1, &g_sub_1_work_q, msg_handler,
					 MSG_ID_MAX_PUB_ID, 0),
	.rx_msgq = &g_rx_msg_queue,
};

static void msg_handler(struct pub_sub_subscriber *subscriber, uint16_t msg_id, const void *msg)
{
	struct test_subscriber *test_subscriber =
		PUB_SUB_CONTAINER_FROM_SUBSCRIBER(subscriber, struct test_subscriber);
	struct rx_msg rx_msg = {
		.subscriber = subscriber,
		.msg_id = msg_id,
		.msg_ptr = msg,
	};
	k_msgq_put(test_subscriber->rx_msgq, &rx_msg, K_FOREVER);
}

static void *delayable_msg_suite_setup(void)
{
	k_work_queue_start(&g_sub_0_work_q, g_sub_0_work_q_stack,
			   K_KERNEL_STACK_SIZEOF(g_sub_0_work_q_stack), -1, NULL);
	k_work_queue_start(&g_sub_1_work_q, g_sub_1_work_q_stack,
			   K_KERNEL_STACK_SIZEOF(g_sub_1_work_q_stack), -1, NULL);

	pub_sub_delayable_msg_init(g_sub_0_msg_0,
				   PUB_SUB_COMPOSED_SUBSCRIBER_PTR(g_test_subscriber_0),
				   MSG_ID_TIMER_0);
	pub_sub_delayable_msg_init(g_sub_0_msg_1,
				   PUB_SUB_COMPOSED_SUBSCRIBER_PTR(g_test_subscriber_0),
				   MSG_ID_TIMER_1);
	pub_sub_delayable_msg_init(g_sub_0_msg_2,
				   PUB_SUB_COMPOSED_SUBSCRIBER_PTR(g_test_subscriber_0),
				   MSG_ID_TIMER_2);
	pub_sub_delayable_msg_init(g_sub_1_msg_0,
				   PUB_SUB_COMPOSED_SUBSCRIBER_PTR(g_test_subscriber_1),
				   MSG_ID_TIMER_0);
	pub_sub_delayable_msg_init(g_sub_1_msg_1,
				   PUB_SUB_COMPOSED_SUBSCRIBER_PTR(g_test_subscriber_1),
				   MSG_ID_TIMER_1);
	pub_sub_delayable_msg_init(g_sub_1_msg_2,
				   PUB_SUB_COMPOSED_SUBSCRIBER_PTR(g_test_subscriber_1),
				   MSG_ID_TIMER_2);

	pub_sub_add_subscriber(PUB_SUB_COMPOSED_SUBSCRIBER_PTR(g_test_subscriber_0));
	pub_sub_add_subscriber(PUB_SUB_COMPOSED_SUBSCRIBER_PTR(g_test_subscriber_1));
	return NULL;
}

static void delayable_msg_after_test(void *fixture)
{
	ARG_UNUSED(fixture);
	// Abort all of the messages and handle any queued messages
	pub_sub_delayable_msg_abort(g_sub_0_msg_0);
	pub_sub_delayable_msg_abort(g_sub_0_msg_1);
	pub_sub_delayable_msg_abort(g_sub_0_msg_2);
	pub_sub_delayable_msg_abort(g_sub_1_msg_0);
	pub_sub_delayable_msg_abort(g_sub_1_msg_1);
	pub_sub_delayable_msg_abort(g_sub_1_msg_2);

	k_thread_resume(k_work_queue_thread_get(&g_sub_0_work_q));
	k_thread_resume(k_work_queue_thread_get(&g_sub_1_work_q));
	// Sleep to let the subscribers run and then purge any messages put into the rx msgq
	k_sleep(K_MSEC(10));
	k_msgq_purge(&g_rx_msg_queue);
}

ZTEST(delayable_msg, test_wait_queue)
{
	struct rx_msg rx_msg;
	struct pub_sub_subscriber *subscriber_0 =
		PUB_SUB_COMPOSED_SUBSCRIBER_PTR(g_test_subscriber_0);
	struct pub_sub_subscriber *subscriber_1 =
		PUB_SUB_COMPOSED_SUBSCRIBER_PTR(g_test_subscriber_1);

	// Start the 6 messages out of order with different timeouts
	pub_sub_delayable_msg_start(g_sub_0_msg_1, K_MSEC(300));
	pub_sub_delayable_msg_start(g_sub_1_msg_1, K_MSEC(400));
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	pub_sub_delayable_msg_start(g_sub_1_msg_2, K_MSEC(600));
	pub_sub_delayable_msg_start(g_sub_0_msg_2, K_MSEC(500));
	pub_sub_delayable_msg_start(g_sub_1_msg_0, K_MSEC(200));

	// Each message should be received at the correct time
	int64_t start_ms = k_uptime_get();
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(100)));
	zassert_equal(k_uptime_get() - start_ms, 100);
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_0, g_sub_0_msg_0);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(100)));
	zassert_equal(k_uptime_get() - start_ms, 200);
	assert_rx_msg(rx_msg, subscriber_1, MSG_ID_TIMER_0, g_sub_1_msg_0);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(100)));
	zassert_equal(k_uptime_get() - start_ms, 300);
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_1, g_sub_0_msg_1);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(100)));
	zassert_equal(k_uptime_get() - start_ms, 400);
	assert_rx_msg(rx_msg, subscriber_1, MSG_ID_TIMER_1, g_sub_1_msg_1);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(100)));
	zassert_equal(k_uptime_get() - start_ms, 500);
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_2, g_sub_0_msg_2);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(100)));
	zassert_equal(k_uptime_get() - start_ms, 600);
	assert_rx_msg(rx_msg, subscriber_1, MSG_ID_TIMER_2, g_sub_1_msg_2);
}

ZTEST(delayable_msg, test_is_active)
{
	struct rx_msg rx_msg;
	struct pub_sub_subscriber *subscriber_0 =
		PUB_SUB_COMPOSED_SUBSCRIBER_PTR(g_test_subscriber_0);

	zassert_false(pub_sub_delayable_msg_is_active(g_sub_0_msg_0));

	// Block subscriber from handling messages
	k_thread_suspend(k_work_queue_thread_get(&g_sub_0_work_q));

	// After starting the message should be active
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	zassert_true(pub_sub_delayable_msg_is_active(g_sub_0_msg_0));

	// After timing out the message should not be active even if it hasn't been handled
	k_sleep(K_MSEC(100));
	zassert_false(pub_sub_delayable_msg_is_active(g_sub_0_msg_0));

	// If the message times out again and is in the expired queue then it should be active
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	zassert_true(pub_sub_delayable_msg_is_active(g_sub_0_msg_0));
	k_sleep(K_MSEC(200));
	zassert_true(pub_sub_delayable_msg_is_active(g_sub_0_msg_0));

	k_thread_resume(k_work_queue_thread_get(&g_sub_0_work_q));
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(1)));
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_0, g_sub_0_msg_0);
	// Message should now be queued with the subscriber so is no longer active
	zassert_false(pub_sub_delayable_msg_is_active(g_sub_0_msg_0));
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_NO_WAIT));
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_0, g_sub_0_msg_0);
	zassert_false(pub_sub_delayable_msg_is_active(g_sub_0_msg_0));
}

ZTEST(delayable_msg, test_same_timeout)
{
	struct rx_msg rx_msg;
	struct pub_sub_subscriber *subscriber_0 =
		PUB_SUB_COMPOSED_SUBSCRIBER_PTR(g_test_subscriber_0);

	// Start 2 messages with the same timeout
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	pub_sub_delayable_msg_start(g_sub_0_msg_1, K_MSEC(100));

	// Each message should be received at the correct time
	int64_t start_ms = k_uptime_get();
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(100)));
	zassert_equal(k_uptime_get() - start_ms, 100);
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_0, g_sub_0_msg_0);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_NO_WAIT));
	zassert_equal(k_uptime_get() - start_ms, 100);
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_1, g_sub_0_msg_1);
}

ZTEST(delayable_msg, test_expired_queue)
{
	struct rx_msg rx_msg;
	struct pub_sub_subscriber *subscriber_0 =
		PUB_SUB_COMPOSED_SUBSCRIBER_PTR(g_test_subscriber_0);
	struct pub_sub_subscriber *subscriber_1 =
		PUB_SUB_COMPOSED_SUBSCRIBER_PTR(g_test_subscriber_1);

	// Start the 6 messages
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	pub_sub_delayable_msg_start(g_sub_0_msg_1, K_MSEC(201));
	pub_sub_delayable_msg_start(g_sub_0_msg_2, K_MSEC(302));

	pub_sub_delayable_msg_start(g_sub_1_msg_0, K_MSEC(50));
	pub_sub_delayable_msg_start(g_sub_1_msg_1, K_MSEC(150));
	pub_sub_delayable_msg_start(g_sub_1_msg_2, K_MSEC(250));

	// Only handle the messages for subscriber 0
	k_thread_suspend(k_work_queue_thread_get(&g_sub_1_work_q));
	int64_t start_ms = k_uptime_get();
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(101)));
	zassert_equal(k_uptime_get() - start_ms, 100);
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_0, g_sub_0_msg_0);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(101)));
	zassert_equal(k_uptime_get() - start_ms, 201);
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_1, g_sub_0_msg_1);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(101)));
	zassert_equal(k_uptime_get() - start_ms, 302);
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_2, g_sub_0_msg_2);

	// Start all of the subscriber 1 messages even though they haven't been handled
	pub_sub_delayable_msg_start(g_sub_1_msg_0, K_MSEC(50));
	pub_sub_delayable_msg_start(g_sub_1_msg_1, K_MSEC(150));
	pub_sub_delayable_msg_start(g_sub_1_msg_2, K_MSEC(250));

	// Queue up more messages for subscriber 0 and handle them
	for (int i = 0; i < 10; i++) {
		start_ms = k_uptime_get();
		pub_sub_delayable_msg_start(g_sub_0_msg_2, K_MSEC(101));
		zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(101)));
		zassert_equal(k_uptime_get() - start_ms, 101);
		assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_2, g_sub_0_msg_2);
	}

	start_ms = k_uptime_get();
	// All of the queued messages can be received by subscriber 1
	k_thread_resume(k_work_queue_thread_get(&g_sub_1_work_q));
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(1)));
	assert_rx_msg(rx_msg, subscriber_1, MSG_ID_TIMER_0, g_sub_1_msg_0);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_NO_WAIT));
	assert_rx_msg(rx_msg, subscriber_1, MSG_ID_TIMER_1, g_sub_1_msg_1);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_NO_WAIT));
	assert_rx_msg(rx_msg, subscriber_1, MSG_ID_TIMER_2, g_sub_1_msg_2);

	zassert_equal(k_uptime_get() - start_ms, 0);

	// All of the expired messages can be received by subscriber 1
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_NO_WAIT));
	assert_rx_msg(rx_msg, subscriber_1, MSG_ID_TIMER_0, g_sub_1_msg_0);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_NO_WAIT));
	assert_rx_msg(rx_msg, subscriber_1, MSG_ID_TIMER_1, g_sub_1_msg_1);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_NO_WAIT));
	assert_rx_msg(rx_msg, subscriber_1, MSG_ID_TIMER_2, g_sub_1_msg_2);

	zassert_equal(k_uptime_get() - start_ms, 0);
}

ZTEST(delayable_msg, test_expired_interleaved)
{
	struct rx_msg rx_msg;
	struct pub_sub_subscriber *subscriber_0 =
		PUB_SUB_COMPOSED_SUBSCRIBER_PTR(g_test_subscriber_0);
	struct pub_sub_subscriber *subscriber_1 =
		PUB_SUB_COMPOSED_SUBSCRIBER_PTR(g_test_subscriber_1);

	k_thread_suspend(k_work_queue_thread_get(&g_sub_0_work_q));
	k_thread_suspend(k_work_queue_thread_get(&g_sub_1_work_q));

	// Start the 6 messages and let them expire
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	pub_sub_delayable_msg_start(g_sub_0_msg_1, K_MSEC(100));
	pub_sub_delayable_msg_start(g_sub_0_msg_2, K_MSEC(100));
	pub_sub_delayable_msg_start(g_sub_1_msg_0, K_MSEC(100));
	pub_sub_delayable_msg_start(g_sub_1_msg_1, K_MSEC(100));
	pub_sub_delayable_msg_start(g_sub_1_msg_2, K_MSEC(100));
	k_sleep(K_MSEC(200));

	// Start the 6 messages again without handling them
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	pub_sub_delayable_msg_start(g_sub_0_msg_1, K_MSEC(200));
	pub_sub_delayable_msg_start(g_sub_0_msg_2, K_MSEC(300));

	pub_sub_delayable_msg_start(g_sub_1_msg_0, K_MSEC(50));
	pub_sub_delayable_msg_start(g_sub_1_msg_1, K_MSEC(150));
	pub_sub_delayable_msg_start(g_sub_1_msg_2, K_MSEC(250));

	// Let them all expire
	k_sleep(K_MSEC(500));

	// Only handle the messages for subscriber 0, both queued and expired
	k_thread_resume(k_work_queue_thread_get(&g_sub_0_work_q));
	// Queued messages
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(1)));
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_0, g_sub_0_msg_0);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_NO_WAIT));
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_1, g_sub_0_msg_1);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_NO_WAIT));
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_2, g_sub_0_msg_2);

	// Expired messages
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_NO_WAIT));
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_0, g_sub_0_msg_0);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_NO_WAIT));
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_1, g_sub_0_msg_1);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_NO_WAIT));
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_2, g_sub_0_msg_2);

	// Handle the queued and expired messages for subscriber 1
	k_thread_resume(k_work_queue_thread_get(&g_sub_1_work_q));

	// Queued messages
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(1)));
	assert_rx_msg(rx_msg, subscriber_1, MSG_ID_TIMER_0, g_sub_1_msg_0);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_NO_WAIT));
	assert_rx_msg(rx_msg, subscriber_1, MSG_ID_TIMER_1, g_sub_1_msg_1);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_NO_WAIT));
	assert_rx_msg(rx_msg, subscriber_1, MSG_ID_TIMER_2, g_sub_1_msg_2);

	// Expired messages
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_NO_WAIT));
	assert_rx_msg(rx_msg, subscriber_1, MSG_ID_TIMER_0, g_sub_1_msg_0);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_NO_WAIT));
	assert_rx_msg(rx_msg, subscriber_1, MSG_ID_TIMER_1, g_sub_1_msg_1);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_NO_WAIT));
	assert_rx_msg(rx_msg, subscriber_1, MSG_ID_TIMER_2, g_sub_1_msg_2);
}

ZTEST(delayable_msg, test_start_msg_with_expired)
{
	struct rx_msg rx_msg;
	struct pub_sub_subscriber *subscriber_0 =
		PUB_SUB_COMPOSED_SUBSCRIBER_PTR(g_test_subscriber_0);
	struct pub_sub_subscriber *subscriber_1 =
		PUB_SUB_COMPOSED_SUBSCRIBER_PTR(g_test_subscriber_1);
	int64_t start_ms;

	// Get a subscriber_1 message into the expired queue
	k_thread_suspend(k_work_queue_thread_get(&g_sub_1_work_q));
	pub_sub_delayable_msg_start(g_sub_1_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(150));
	pub_sub_delayable_msg_start(g_sub_1_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(150));

	// Start a message for subscriber_0
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));

	start_ms = k_uptime_get();

	// Subscriber 1 should receive its queued and expired message
	k_thread_resume(k_work_queue_thread_get(&g_sub_1_work_q));
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(1)));
	assert_rx_msg(rx_msg, subscriber_1, MSG_ID_TIMER_0, g_sub_1_msg_0);
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_NO_WAIT));
	assert_rx_msg(rx_msg, subscriber_1, MSG_ID_TIMER_0, g_sub_1_msg_0);
	zassert_equal(k_uptime_get() - start_ms, 0);

	// Get subscriber_1 message back into the expired queue within subscriber_0's message
	// timeout
	k_thread_suspend(k_work_queue_thread_get(&g_sub_1_work_q));
	pub_sub_delayable_msg_start(g_sub_1_msg_0, K_MSEC(1));
	k_sleep(K_MSEC(2));
	pub_sub_delayable_msg_start(g_sub_1_msg_0, K_MSEC(1));
	k_sleep(K_MSEC(2));

	// subscriber_0's message should be received at the correct time
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(100)));
	zassert_equal(k_uptime_get() - start_ms, 100);
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_0, g_sub_0_msg_0);

	// Subscriber 1 should receive its queued and expired message
	start_ms = k_uptime_get();
	k_thread_resume(k_work_queue_thread_get(&g_sub_1_work_q));
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(1)));
	assert_rx_msg(rx_msg, subscriber_1, MSG_ID_TIMER_0, g_sub_1_msg_0);
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_NO_WAIT));
	assert_rx_msg(rx_msg, subscriber_1, MSG_ID_TIMER_0, g_sub_1_msg_0);
	zassert_equal(k_uptime_get() - start_ms, 0);
}

ZTEST(delayable_msg, test_abort_msg)
{
	struct rx_msg rx_msg;
	struct pub_sub_subscriber *subscriber_0 =
		PUB_SUB_COMPOSED_SUBSCRIBER_PTR(g_test_subscriber_0);
	int64_t start_ms;

	// Start a message
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));

	// Let it almost expire and then abort it
	k_sleep(K_MSEC(80));
	pub_sub_delayable_msg_abort(g_sub_0_msg_0);
	zassert_false(pub_sub_delayable_msg_was_aborted(g_sub_0_msg_0));

	// The message should not be received
	zassert_not_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(100)));

	// Start two messages
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	pub_sub_delayable_msg_start(g_sub_0_msg_1, K_MSEC(200));

	start_ms = k_uptime_get();

	// Let the first message almost expire and then abort it
	k_sleep(K_MSEC(80));
	pub_sub_delayable_msg_abort(g_sub_0_msg_0);
	zassert_false(pub_sub_delayable_msg_was_aborted(g_sub_0_msg_0));

	// The second message should be received at the correct time
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(200)));
	zassert_equal(k_uptime_get() - start_ms, 200);
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_1, g_sub_0_msg_1);

	// Start three messages
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	pub_sub_delayable_msg_start(g_sub_0_msg_1, K_MSEC(200));
	pub_sub_delayable_msg_start(g_sub_0_msg_2, K_MSEC(300));

	start_ms = k_uptime_get();

	// Let the first message almost expire and then abort the second one
	k_sleep(K_MSEC(80));
	pub_sub_delayable_msg_abort(g_sub_0_msg_1);
	zassert_false(pub_sub_delayable_msg_was_aborted(g_sub_0_msg_1));

	// The first message should be received at the correct time
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(100)));
	zassert_equal(k_uptime_get() - start_ms, 100);
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_0, g_sub_0_msg_0);

	// The third message should be received at the correct time
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(300)));
	zassert_equal(k_uptime_get() - start_ms, 300);
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_2, g_sub_0_msg_2);
}

ZTEST(delayable_msg, test_abort_msg_with_expired)
{
	struct rx_msg rx_msg;
	struct pub_sub_subscriber *subscriber_0 =
		PUB_SUB_COMPOSED_SUBSCRIBER_PTR(g_test_subscriber_0);
	struct pub_sub_subscriber *subscriber_1 =
		PUB_SUB_COMPOSED_SUBSCRIBER_PTR(g_test_subscriber_1);
	int64_t start_ms;

	// Start a message and let it expire then start it again and let it expire
	k_thread_suspend(k_work_queue_thread_get(&g_sub_1_work_q));
	pub_sub_delayable_msg_start(g_sub_1_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(150));
	pub_sub_delayable_msg_start(g_sub_1_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(150));

	// Start a message
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));

	// Let it almost expire and then abort it
	k_sleep(K_MSEC(80));
	pub_sub_delayable_msg_abort(g_sub_0_msg_0);
	zassert_false(pub_sub_delayable_msg_was_aborted(g_sub_0_msg_0));

	// The message should not be received
	zassert_not_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(100)));

	// Start two messages
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	pub_sub_delayable_msg_start(g_sub_0_msg_1, K_MSEC(200));

	start_ms = k_uptime_get();

	// Let the first message almost expire and then abort it
	k_sleep(K_MSEC(80));
	pub_sub_delayable_msg_abort(g_sub_0_msg_0);
	zassert_false(pub_sub_delayable_msg_was_aborted(g_sub_0_msg_0));

	// The second message should be received at the correct time
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(200)));
	zassert_equal(k_uptime_get() - start_ms, 200);
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_1, g_sub_0_msg_1);

	// Start three messages
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	pub_sub_delayable_msg_start(g_sub_0_msg_1, K_MSEC(200));
	pub_sub_delayable_msg_start(g_sub_0_msg_2, K_MSEC(300));

	start_ms = k_uptime_get();

	// Let the first message almost expire and then abort the second one
	k_sleep(K_MSEC(80));
	pub_sub_delayable_msg_abort(g_sub_0_msg_1);
	zassert_false(pub_sub_delayable_msg_was_aborted(g_sub_0_msg_1));

	// The first message should be received at the correct time
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(100)));
	zassert_equal(k_uptime_get() - start_ms, 100);
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_0, g_sub_0_msg_0);

	// The third message should be received at the correct time
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(300)));
	zassert_equal(k_uptime_get() - start_ms, 300);
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_2, g_sub_0_msg_2);

	// The queued and expired message should be able to be received
	k_thread_resume(k_work_queue_thread_get(&g_sub_1_work_q));
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(1)));
	assert_rx_msg(rx_msg, subscriber_1, MSG_ID_TIMER_0, g_sub_1_msg_0);
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_NO_WAIT));
	assert_rx_msg(rx_msg, subscriber_1, MSG_ID_TIMER_0, g_sub_1_msg_0);
}

ZTEST(delayable_msg, test_abort_queued_msg)
{
	struct rx_msg rx_msg;
	struct pub_sub_subscriber *subscriber_0 =
		PUB_SUB_COMPOSED_SUBSCRIBER_PTR(g_test_subscriber_0);

	// Start a message and let it expire but don't let it be handled
	k_thread_suspend(k_work_queue_thread_get(&g_sub_0_work_q));
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(100));

	// Aborting the message should set was aborted to true
	pub_sub_delayable_msg_abort(g_sub_0_msg_0);
	zassert_true(pub_sub_delayable_msg_was_aborted(g_sub_0_msg_0));

	// The message should be received
	k_thread_resume(k_work_queue_thread_get(&g_sub_0_work_q));
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(1)));
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_0, g_sub_0_msg_0);
	// Was aborted should return false after the aborted message has been handled
	zassert_false(pub_sub_delayable_msg_was_aborted(g_sub_0_msg_0));

	// Start a message and let it expire twice without being handled
	k_thread_suspend(k_work_queue_thread_get(&g_sub_0_work_q));
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(100));
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(100));

	// Aborting the message should set was aborted to true
	pub_sub_delayable_msg_abort(g_sub_0_msg_0);
	zassert_true(pub_sub_delayable_msg_was_aborted(g_sub_0_msg_0));

	// The message should be received once
	k_thread_resume(k_work_queue_thread_get(&g_sub_0_work_q));
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(1)));
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_0, g_sub_0_msg_0);
	// Was aborted should return false after the aborted message has been handled
	zassert_false(pub_sub_delayable_msg_was_aborted(g_sub_0_msg_0));
	zassert_not_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(200)));
}

ZTEST(delayable_msg, test_update_single_msg)
{
	struct rx_msg rx_msg;
	struct pub_sub_subscriber *subscriber_0 =
		PUB_SUB_COMPOSED_SUBSCRIBER_PTR(g_test_subscriber_0);
	int64_t start_ms;

	// Start a message and pass some time
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(50));

	// Update the timeout to be later
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(200));
	zassert_false(pub_sub_delayable_msg_was_aborted(g_sub_0_msg_0));
	start_ms = k_uptime_get();

	// The message should be received at the correct time
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(250)));
	zassert_equal(k_uptime_get() - start_ms, 200);
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_0, g_sub_0_msg_0);

	// Start a message and pass some time
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(200));
	k_sleep(K_MSEC(50));

	// Update the timeout to be earlier
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	zassert_false(pub_sub_delayable_msg_was_aborted(g_sub_0_msg_0));
	start_ms = k_uptime_get();

	// The message should be received at the correct time
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(250)));
	zassert_equal(k_uptime_get() - start_ms, 100);
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_0, g_sub_0_msg_0);
}

ZTEST(delayable_msg, test_update_multi_msg)
{
	struct rx_msg rx_msg;
	struct pub_sub_subscriber *subscriber_0 =
		PUB_SUB_COMPOSED_SUBSCRIBER_PTR(g_test_subscriber_0);
	int64_t start_ms;

	// Start three messages and pass some time
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(150));
	pub_sub_delayable_msg_start(g_sub_0_msg_1, K_MSEC(250));
	pub_sub_delayable_msg_start(g_sub_0_msg_2, K_MSEC(350));
	k_sleep(K_MSEC(50));

	// Update the message timeouts
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(250));
	zassert_false(pub_sub_delayable_msg_was_aborted(g_sub_0_msg_0));
	pub_sub_delayable_msg_start(g_sub_0_msg_1, K_MSEC(150));
	zassert_false(pub_sub_delayable_msg_was_aborted(g_sub_0_msg_1));
	pub_sub_delayable_msg_start(g_sub_0_msg_2, K_MSEC(50));
	zassert_false(pub_sub_delayable_msg_was_aborted(g_sub_0_msg_2));
	start_ms = k_uptime_get();

	// The messages should be received at the correct time
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(250)));
	zassert_equal(k_uptime_get() - start_ms, 50);
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_2, g_sub_0_msg_2);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(250)));
	zassert_equal(k_uptime_get() - start_ms, 150);
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_1, g_sub_0_msg_1);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(250)));
	zassert_equal(k_uptime_get() - start_ms, 250);
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_0, g_sub_0_msg_0);
}

ZTEST(delayable_msg, test_update_single_msg_with_expired)
{
	struct rx_msg rx_msg;
	struct pub_sub_subscriber *subscriber_0 =
		PUB_SUB_COMPOSED_SUBSCRIBER_PTR(g_test_subscriber_0);
	int64_t start_ms;

	// Start a message and let it expire then start it again and let it expire
	k_thread_suspend(k_work_queue_thread_get(&g_sub_1_work_q));
	pub_sub_delayable_msg_start(g_sub_1_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(150));
	pub_sub_delayable_msg_start(g_sub_1_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(150));

	// Start a message and pass some time
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(50));

	// Update the timeout to be later
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(200));
	zassert_false(pub_sub_delayable_msg_was_aborted(g_sub_0_msg_0));
	start_ms = k_uptime_get();

	// The message should be received at the correct time
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(250)));
	zassert_equal(k_uptime_get() - start_ms, 200);
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_0, g_sub_0_msg_0);

	// Start a message and pass some time
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(200));
	k_sleep(K_MSEC(50));

	// Update the timeout to be earlier
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	zassert_false(pub_sub_delayable_msg_was_aborted(g_sub_0_msg_0));
	start_ms = k_uptime_get();

	// The message should be received at the correct time
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(250)));
	zassert_equal(k_uptime_get() - start_ms, 100);
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_0, g_sub_0_msg_0);
}

ZTEST(delayable_msg, test_update_multi_msg_with_expired)
{
	struct rx_msg rx_msg;
	struct pub_sub_subscriber *subscriber_0 =
		PUB_SUB_COMPOSED_SUBSCRIBER_PTR(g_test_subscriber_0);
	int64_t start_ms;

	// Start a message and let it expire then start it again and let it expire
	k_thread_suspend(k_work_queue_thread_get(&g_sub_1_work_q));
	pub_sub_delayable_msg_start(g_sub_1_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(150));
	pub_sub_delayable_msg_start(g_sub_1_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(150));

	// Start three messages and pass some time
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(150));
	pub_sub_delayable_msg_start(g_sub_0_msg_1, K_MSEC(250));
	pub_sub_delayable_msg_start(g_sub_0_msg_2, K_MSEC(350));
	k_sleep(K_MSEC(50));

	// Update the message timeouts
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(250));
	zassert_false(pub_sub_delayable_msg_was_aborted(g_sub_0_msg_0));
	pub_sub_delayable_msg_start(g_sub_0_msg_1, K_MSEC(150));
	zassert_false(pub_sub_delayable_msg_was_aborted(g_sub_0_msg_1));
	pub_sub_delayable_msg_start(g_sub_0_msg_2, K_MSEC(50));
	zassert_false(pub_sub_delayable_msg_was_aborted(g_sub_0_msg_2));
	start_ms = k_uptime_get();

	// The messages should be received at the correct time
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(250)));
	zassert_equal(k_uptime_get() - start_ms, 50);
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_2, g_sub_0_msg_2);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(250)));
	zassert_equal(k_uptime_get() - start_ms, 150);
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_1, g_sub_0_msg_1);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(250)));
	zassert_equal(k_uptime_get() - start_ms, 250);
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_0, g_sub_0_msg_0);
}

ZTEST(delayable_msg, test_update_queued_msg)
{
	struct rx_msg rx_msg;
	struct pub_sub_subscriber *subscriber_0 =
		PUB_SUB_COMPOSED_SUBSCRIBER_PTR(g_test_subscriber_0);
	int64_t start_ms;

	// Start a message and let it expire
	k_thread_suspend(k_work_queue_thread_get(&g_sub_0_work_q));
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(100));

	// Updating timeout without handling the message should set was aborted to true
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(200));
	zassert_true(pub_sub_delayable_msg_was_aborted(g_sub_0_msg_0));
	start_ms = k_uptime_get();

	// The queued message should be received immediately
	k_thread_resume(k_work_queue_thread_get(&g_sub_0_work_q));
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(1)));
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_0, g_sub_0_msg_0);
	// Was aborted should return false after the restarted message has been handled
	zassert_false(pub_sub_delayable_msg_was_aborted(g_sub_0_msg_0));

	// The message should also be received after the updated timeout
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(250)));
	zassert_equal(k_uptime_get() - start_ms, 200);
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_0, g_sub_0_msg_0);

	// Start the message and let it expire twice without handling it
	k_thread_suspend(k_work_queue_thread_get(&g_sub_0_work_q));
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(100));
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(100));

	// Updating timeout without handling the message should set was aborted to true
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(200));
	zassert_true(pub_sub_delayable_msg_was_aborted(g_sub_0_msg_0));
	start_ms = k_uptime_get();

	// The queued message should be received immediately
	k_thread_resume(k_work_queue_thread_get(&g_sub_0_work_q));
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(1)));
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_0, g_sub_0_msg_0);
	// Was aborted should return false after the restarted message has been handled
	zassert_false(pub_sub_delayable_msg_was_aborted(g_sub_0_msg_0));

	// The message should also be received after the updated timeout
	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(250)));
	zassert_equal(k_uptime_get() - start_ms, 200);
	assert_rx_msg(rx_msg, subscriber_0, MSG_ID_TIMER_0, g_sub_0_msg_0);
}

ZTEST_SUITE(delayable_msg, NULL, delayable_msg_suite_setup, NULL, delayable_msg_after_test, NULL);
