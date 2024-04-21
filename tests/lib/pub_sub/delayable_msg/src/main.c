/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <pub_sub/pub_sub.h>
#include <pub_sub/delayable_msg.h>
#include <zephyr/ztest.h>
#include <stdlib.h>
#include <helpers.h>

enum msg_id {
	MSG_ID_SUBSCRIBED_ID_0,
	MSG_ID_MAX_PUB_ID = MSG_ID_SUBSCRIBED_ID_0,
	MSG_ID_TIMER_0,
	MSG_ID_TIMER_1,
	MSG_ID_TIMER_2,
};

struct test_msg {
};

struct expected_msg {
	uint16_t msg_id;
	void *msg;
};

PUB_SUB_STATIC_DELAYABLE_MSG_DEFINE(struct test_msg, g_sub_0_msg_0, MSG_ID_TIMER_0, NULL);
PUB_SUB_STATIC_DELAYABLE_MSG_DEFINE(struct test_msg, g_sub_0_msg_1, MSG_ID_TIMER_1, NULL);
PUB_SUB_STATIC_DELAYABLE_MSG_DEFINE(struct test_msg, g_sub_0_msg_2, MSG_ID_TIMER_2, NULL);
PUB_SUB_STATIC_DELAYABLE_MSG_DEFINE(struct test_msg, g_sub_1_msg_0, MSG_ID_TIMER_0, NULL);
PUB_SUB_STATIC_DELAYABLE_MSG_DEFINE(struct test_msg, g_sub_1_msg_1, MSG_ID_TIMER_1, NULL);
PUB_SUB_STATIC_DELAYABLE_MSG_DEFINE(struct test_msg, g_sub_1_msg_2, MSG_ID_TIMER_2, NULL);
struct fifo_subscriber *g_test_subscriber_0;
struct fifo_subscriber *g_test_subscriber_1;
struct expected_msg g_sub_0_expected_msg;
struct expected_msg g_sub_1_expected_msg;

static void msg_handler(uint16_t msg_id, const void *msg, void *user_data)
{
	struct expected_msg *expected_msg = user_data;
	zassert_equal(msg_id, expected_msg->msg_id);
	zassert_equal_ptr(msg, expected_msg->msg);
}

static void after_test_msg_handler(uint16_t msg_id, const void *msg, void *user_data)
{
}

static void *delayable_msg_suite_setup(void)
{
	reset_default_broker();
	g_test_subscriber_0 = malloc_fifo_subscriber(MSG_ID_MAX_PUB_ID);
	g_test_subscriber_1 = malloc_fifo_subscriber(MSG_ID_MAX_PUB_ID);

	pub_sub_delayable_msg_init(g_sub_0_msg_0, &g_test_subscriber_0->subscriber, MSG_ID_TIMER_0);
	pub_sub_delayable_msg_init(g_sub_0_msg_1, &g_test_subscriber_0->subscriber, MSG_ID_TIMER_1);
	pub_sub_delayable_msg_init(g_sub_0_msg_2, &g_test_subscriber_0->subscriber, MSG_ID_TIMER_2);
	pub_sub_delayable_msg_init(g_sub_1_msg_0, &g_test_subscriber_1->subscriber, MSG_ID_TIMER_0);
	pub_sub_delayable_msg_init(g_sub_1_msg_1, &g_test_subscriber_1->subscriber, MSG_ID_TIMER_1);
	pub_sub_delayable_msg_init(g_sub_1_msg_2, &g_test_subscriber_1->subscriber, MSG_ID_TIMER_2);

	pub_sub_subscriber_set_handler_data(&g_test_subscriber_0->subscriber, msg_handler,
					    &g_sub_0_expected_msg);
	pub_sub_subscriber_set_handler_data(&g_test_subscriber_1->subscriber, msg_handler,
					    &g_sub_1_expected_msg);
	pub_sub_add_subscriber(&g_test_subscriber_0->subscriber);
	pub_sub_add_subscriber(&g_test_subscriber_1->subscriber);
	return NULL;
}

static void delayable_msg_suite_teardown(void *fixture)
{
	reset_default_broker();
}

static void delayable_msg_before_test(void *fixture)
{
	pub_sub_subscriber_set_handler_data(&g_test_subscriber_0->subscriber, msg_handler,
					    &g_sub_0_expected_msg);
	pub_sub_subscriber_set_handler_data(&g_test_subscriber_1->subscriber, msg_handler,
					    &g_sub_1_expected_msg);
}

static void delayable_msg_after_test(void *fixture)
{
	ARG_UNUSED(fixture);
	// Abort all of the messages and handle any queued messages
	(void)pub_sub_delayable_msg_abort(g_sub_0_msg_0);
	(void)pub_sub_delayable_msg_abort(g_sub_0_msg_1);
	(void)pub_sub_delayable_msg_abort(g_sub_0_msg_2);
	(void)pub_sub_delayable_msg_abort(g_sub_1_msg_0);
	(void)pub_sub_delayable_msg_abort(g_sub_1_msg_1);
	(void)pub_sub_delayable_msg_abort(g_sub_1_msg_2);
	pub_sub_subscriber_set_handler_data(&g_test_subscriber_0->subscriber,
					    after_test_msg_handler, NULL);
	pub_sub_subscriber_set_handler_data(&g_test_subscriber_1->subscriber,
					    after_test_msg_handler, NULL);
	while (pub_sub_handle_queued_msg(&g_test_subscriber_0->subscriber, K_NO_WAIT) == 0) {
	}
	while (pub_sub_handle_queued_msg(&g_test_subscriber_1->subscriber, K_NO_WAIT) == 0) {
	}
}

ZTEST(delayable_msg, test_wait_queue)
{
	struct pub_sub_subscriber *subscriber_0 = &g_test_subscriber_0->subscriber;
	struct pub_sub_subscriber *subscriber_1 = &g_test_subscriber_1->subscriber;
	int ret;

	// Start the 6 messages out of order with different timeouts
	pub_sub_delayable_msg_start(g_sub_0_msg_1, K_MSEC(300));
	pub_sub_delayable_msg_start(g_sub_1_msg_1, K_MSEC(400));
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	pub_sub_delayable_msg_start(g_sub_1_msg_2, K_MSEC(600));
	pub_sub_delayable_msg_start(g_sub_0_msg_2, K_MSEC(500));
	pub_sub_delayable_msg_start(g_sub_1_msg_0, K_MSEC(200));

	// Each message should be received at the correct time
	int64_t start_ms = k_uptime_get();
	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_0;
	g_sub_0_expected_msg.msg = g_sub_0_msg_0;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(100));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 100);

	g_sub_1_expected_msg.msg_id = MSG_ID_TIMER_0;
	g_sub_1_expected_msg.msg = g_sub_1_msg_0;
	ret = pub_sub_handle_queued_msg(subscriber_1, K_MSEC(100));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 200);

	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_1;
	g_sub_0_expected_msg.msg = g_sub_0_msg_1;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(100));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 300);

	g_sub_1_expected_msg.msg_id = MSG_ID_TIMER_1;
	g_sub_1_expected_msg.msg = g_sub_1_msg_1;
	ret = pub_sub_handle_queued_msg(subscriber_1, K_MSEC(100));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 400);

	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_2;
	g_sub_0_expected_msg.msg = g_sub_0_msg_2;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(100));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 500);

	g_sub_1_expected_msg.msg_id = MSG_ID_TIMER_2;
	g_sub_1_expected_msg.msg = g_sub_1_msg_2;
	ret = pub_sub_handle_queued_msg(subscriber_1, K_MSEC(100));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 600);
}

ZTEST(delayable_msg, test_is_active)
{
	struct pub_sub_subscriber *subscriber_0 = &g_test_subscriber_0->subscriber;
	int ret;

	zassert_false(pub_sub_delayable_msg_is_active(g_sub_0_msg_0));

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

	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_0;
	g_sub_0_expected_msg.msg = g_sub_0_msg_0;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_NO_WAIT);
	zassert_ok(ret);
	// Message should still be in the expired queue so should still be active
	zassert_true(pub_sub_delayable_msg_is_active(g_sub_0_msg_0));
	k_sleep(K_MSEC(5));
	// Message should now be queued with the subscriber is is no longer active
	zassert_false(pub_sub_delayable_msg_is_active(g_sub_0_msg_0));
	ret = pub_sub_handle_queued_msg(subscriber_0, K_NO_WAIT);
	zassert_ok(ret);
	zassert_false(pub_sub_delayable_msg_is_active(g_sub_0_msg_0));
}

ZTEST(delayable_msg, test_same_timeout)
{
	struct pub_sub_subscriber *subscriber_0 = &g_test_subscriber_0->subscriber;
	int ret;

	// Start 2 messages with the same timeout
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	pub_sub_delayable_msg_start(g_sub_0_msg_1, K_MSEC(100));

	// Each message should be received at the correct time
	int64_t start_ms = k_uptime_get();
	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_0;
	g_sub_0_expected_msg.msg = g_sub_0_msg_0;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(100));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 100);

	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_1;
	g_sub_0_expected_msg.msg = g_sub_0_msg_1;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_NO_WAIT);
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 100);
}

ZTEST(delayable_msg, test_expired_queue)
{
	struct pub_sub_subscriber *subscriber_0 = &g_test_subscriber_0->subscriber;
	struct pub_sub_subscriber *subscriber_1 = &g_test_subscriber_1->subscriber;
	int ret;

	// Start the 6 messages
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	pub_sub_delayable_msg_start(g_sub_0_msg_1, K_MSEC(201));
	pub_sub_delayable_msg_start(g_sub_0_msg_2, K_MSEC(302));

	pub_sub_delayable_msg_start(g_sub_1_msg_0, K_MSEC(50));
	pub_sub_delayable_msg_start(g_sub_1_msg_1, K_MSEC(150));
	pub_sub_delayable_msg_start(g_sub_1_msg_2, K_MSEC(250));

	// Only handle the messages for subscriber 0
	int64_t start_ms = k_uptime_get();
	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_0;
	g_sub_0_expected_msg.msg = g_sub_0_msg_0;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(101));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 100);

	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_1;
	g_sub_0_expected_msg.msg = g_sub_0_msg_1;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(101));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 201);

	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_2;
	g_sub_0_expected_msg.msg = g_sub_0_msg_2;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(101));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 302);

	// Start all of the subscriber 1 messages even though they haven't been handled
	pub_sub_delayable_msg_start(g_sub_1_msg_0, K_MSEC(50));
	pub_sub_delayable_msg_start(g_sub_1_msg_1, K_MSEC(150));
	pub_sub_delayable_msg_start(g_sub_1_msg_2, K_MSEC(250));

	// Queue up more messages for subscriber 0 and handle them
	for (int i = 0; i < 10; i++) {
		start_ms = k_uptime_get();
		pub_sub_delayable_msg_start(g_sub_0_msg_2, K_MSEC(101));
		ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(101));
		zassert_ok(ret);
		zassert_equal(k_uptime_get() - start_ms, 101);
	}

	start_ms = k_uptime_get();
	// All of the queued messages can be received by subscriber 1
	g_sub_1_expected_msg.msg_id = MSG_ID_TIMER_0;
	g_sub_1_expected_msg.msg = g_sub_1_msg_0;
	ret = pub_sub_handle_queued_msg(subscriber_1, K_NO_WAIT);
	zassert_ok(ret);

	g_sub_1_expected_msg.msg_id = MSG_ID_TIMER_1;
	g_sub_1_expected_msg.msg = g_sub_1_msg_1;
	ret = pub_sub_handle_queued_msg(subscriber_1, K_NO_WAIT);
	zassert_ok(ret);

	g_sub_1_expected_msg.msg_id = MSG_ID_TIMER_2;
	g_sub_1_expected_msg.msg = g_sub_1_msg_2;
	ret = pub_sub_handle_queued_msg(subscriber_1, K_NO_WAIT);
	zassert_ok(ret);

	zassert_equal(k_uptime_get() - start_ms, 0);

	// All of the expired messages can be received by subscriber 1
	g_sub_1_expected_msg.msg_id = MSG_ID_TIMER_0;
	g_sub_1_expected_msg.msg = g_sub_1_msg_0;
	ret = pub_sub_handle_queued_msg(subscriber_1, K_MSEC(5));
	zassert_ok(ret);

	g_sub_1_expected_msg.msg_id = MSG_ID_TIMER_1;
	g_sub_1_expected_msg.msg = g_sub_1_msg_1;
	ret = pub_sub_handle_queued_msg(subscriber_1, K_NO_WAIT);
	zassert_ok(ret);

	g_sub_1_expected_msg.msg_id = MSG_ID_TIMER_2;
	g_sub_1_expected_msg.msg = g_sub_1_msg_2;
	ret = pub_sub_handle_queued_msg(subscriber_1, K_NO_WAIT);
	zassert_ok(ret);
	// Expired messages are re-checked every 5 ms
	zassert_equal(k_uptime_get() - start_ms, 5);
}

ZTEST(delayable_msg, test_expired_interleaved)
{
	struct pub_sub_subscriber *subscriber_0 = &g_test_subscriber_0->subscriber;
	struct pub_sub_subscriber *subscriber_1 = &g_test_subscriber_1->subscriber;
	int ret;

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
	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_0;
	g_sub_0_expected_msg.msg = g_sub_0_msg_0;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_NO_WAIT);
	zassert_ok(ret);

	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_1;
	g_sub_0_expected_msg.msg = g_sub_0_msg_1;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_NO_WAIT);
	zassert_ok(ret);

	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_2;
	g_sub_0_expected_msg.msg = g_sub_0_msg_2;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_NO_WAIT);
	zassert_ok(ret);
	// Expired messages should be received within 5 ms
	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_0;
	g_sub_0_expected_msg.msg = g_sub_0_msg_0;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(5));
	zassert_ok(ret);

	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_1;
	g_sub_0_expected_msg.msg = g_sub_0_msg_1;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_NO_WAIT);
	zassert_ok(ret);

	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_2;
	g_sub_0_expected_msg.msg = g_sub_0_msg_2;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_NO_WAIT);
	zassert_ok(ret);

	// Handle the queued and expired messages for subscriber 1
	g_sub_1_expected_msg.msg_id = MSG_ID_TIMER_0;
	g_sub_1_expected_msg.msg = g_sub_1_msg_0;
	ret = pub_sub_handle_queued_msg(subscriber_1, K_NO_WAIT);
	zassert_ok(ret);

	g_sub_1_expected_msg.msg_id = MSG_ID_TIMER_1;
	g_sub_1_expected_msg.msg = g_sub_1_msg_1;
	ret = pub_sub_handle_queued_msg(subscriber_1, K_NO_WAIT);
	zassert_ok(ret);

	g_sub_1_expected_msg.msg_id = MSG_ID_TIMER_2;
	g_sub_1_expected_msg.msg = g_sub_1_msg_2;
	ret = pub_sub_handle_queued_msg(subscriber_1, K_NO_WAIT);
	zassert_ok(ret);
	// Expired messages should be received within 5 ms
	g_sub_1_expected_msg.msg_id = MSG_ID_TIMER_0;
	g_sub_1_expected_msg.msg = g_sub_1_msg_0;
	ret = pub_sub_handle_queued_msg(subscriber_1, K_MSEC(5));
	zassert_ok(ret);

	g_sub_1_expected_msg.msg_id = MSG_ID_TIMER_1;
	g_sub_1_expected_msg.msg = g_sub_1_msg_1;
	ret = pub_sub_handle_queued_msg(subscriber_1, K_NO_WAIT);
	zassert_ok(ret);

	g_sub_1_expected_msg.msg_id = MSG_ID_TIMER_2;
	g_sub_1_expected_msg.msg = g_sub_1_msg_2;
	ret = pub_sub_handle_queued_msg(subscriber_1, K_NO_WAIT);
	zassert_ok(ret);
}

ZTEST(delayable_msg, test_start_msg_with_expired)
{
	struct pub_sub_subscriber *subscriber_0 = &g_test_subscriber_0->subscriber;
	struct pub_sub_subscriber *subscriber_1 = &g_test_subscriber_1->subscriber;
	int ret;
	int64_t start_ms;

	// Get a subscriber_1 message into the expired queue
	pub_sub_delayable_msg_start(g_sub_1_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(150));
	pub_sub_delayable_msg_start(g_sub_1_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(150));

	// Start a message for subscriber_0
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));

	start_ms = k_uptime_get();

	// Subscriber 1 should receive its expired message within 5 ms
	g_sub_1_expected_msg.msg_id = MSG_ID_TIMER_0;
	g_sub_1_expected_msg.msg = g_sub_1_msg_0;
	ret = pub_sub_handle_queued_msg(subscriber_1, K_NO_WAIT);
	zassert_ok(ret);
	ret = pub_sub_handle_queued_msg(subscriber_1, K_MSEC(5));
	zassert_ok(ret);
	zassert_true((k_uptime_get() - start_ms) <= 5);

	// Get subscriber_1 message back into the expired queue within subscriber_0's message
	// timeout
	pub_sub_delayable_msg_start(g_sub_1_msg_0, K_MSEC(1));
	k_sleep(K_MSEC(3));
	pub_sub_delayable_msg_start(g_sub_1_msg_0, K_MSEC(1));
	k_sleep(K_MSEC(3));

	// subscriber_0's message should be received at the correct time
	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_0;
	g_sub_0_expected_msg.msg = g_sub_0_msg_0;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(100));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 100);

	// Subscriber 1 should receive its expired message within 5 ms
	start_ms = k_uptime_get();
	ret = pub_sub_handle_queued_msg(subscriber_1, K_NO_WAIT);
	zassert_ok(ret);
	ret = pub_sub_handle_queued_msg(subscriber_1, K_MSEC(5));
	zassert_ok(ret);
	zassert_true((k_uptime_get() - start_ms) <= 5);
}

ZTEST(delayable_msg, test_abort_msg)
{
	struct pub_sub_subscriber *subscriber_0 = &g_test_subscriber_0->subscriber;
	int ret;
	int64_t start_ms;

	// Start a message
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));

	// Let it almost expire and then abort it
	k_sleep(K_MSEC(80));
	zassert_ok(pub_sub_delayable_msg_abort(g_sub_0_msg_0));

	// The message should not be received
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(100));
	zassert_not_ok(ret);

	// Start two messages
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	pub_sub_delayable_msg_start(g_sub_0_msg_1, K_MSEC(200));

	start_ms = k_uptime_get();

	// Let the first message almost expire and then abort it
	k_sleep(K_MSEC(80));
	zassert_ok(pub_sub_delayable_msg_abort(g_sub_0_msg_0));

	// The second message should be received at the correct time
	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_1;
	g_sub_0_expected_msg.msg = g_sub_0_msg_1;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(200));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 200);

	// Start three messages
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	pub_sub_delayable_msg_start(g_sub_0_msg_1, K_MSEC(200));
	pub_sub_delayable_msg_start(g_sub_0_msg_2, K_MSEC(300));

	start_ms = k_uptime_get();

	// Let the first message almost expire and then abort the second one
	k_sleep(K_MSEC(80));
	zassert_ok(pub_sub_delayable_msg_abort(g_sub_0_msg_1));

	// The first message should be received at the correct time
	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_0;
	g_sub_0_expected_msg.msg = g_sub_0_msg_0;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(100));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 100);

	// The third message should be received at the correct time
	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_2;
	g_sub_0_expected_msg.msg = g_sub_0_msg_2;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(300));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 300);
}

ZTEST(delayable_msg, test_abort_msg_with_expired)
{
	struct pub_sub_subscriber *subscriber_0 = &g_test_subscriber_0->subscriber;
	struct pub_sub_subscriber *subscriber_1 = &g_test_subscriber_1->subscriber;
	int ret;
	int64_t start_ms;

	// Start a message and let it expire then start it again and let it expire
	pub_sub_delayable_msg_start(g_sub_1_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(150));
	pub_sub_delayable_msg_start(g_sub_1_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(150));

	// Start a message
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));

	// Let it almost expire and then abort it
	k_sleep(K_MSEC(80));
	zassert_ok(pub_sub_delayable_msg_abort(g_sub_0_msg_0));

	// The message should not be received
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(100));
	zassert_not_ok(ret);

	// Start two messages
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	pub_sub_delayable_msg_start(g_sub_0_msg_1, K_MSEC(200));

	start_ms = k_uptime_get();

	// Let the first message almost expire and then abort it
	k_sleep(K_MSEC(80));
	zassert_ok(pub_sub_delayable_msg_abort(g_sub_0_msg_0));

	// The second message should be received at the correct time
	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_1;
	g_sub_0_expected_msg.msg = g_sub_0_msg_1;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(200));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 200);

	// Start three messages
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	pub_sub_delayable_msg_start(g_sub_0_msg_1, K_MSEC(200));
	pub_sub_delayable_msg_start(g_sub_0_msg_2, K_MSEC(300));

	start_ms = k_uptime_get();

	// Let the first message almost expire and then abort the second one
	k_sleep(K_MSEC(80));
	zassert_ok(pub_sub_delayable_msg_abort(g_sub_0_msg_1));

	// The first message should be received at the correct time
	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_0;
	g_sub_0_expected_msg.msg = g_sub_0_msg_0;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(100));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 100);

	// The third message should be received at the correct time
	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_2;
	g_sub_0_expected_msg.msg = g_sub_0_msg_2;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(300));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 300);

	// The queued and expired message should be able to be received
	g_sub_1_expected_msg.msg_id = MSG_ID_TIMER_0;
	g_sub_1_expected_msg.msg = g_sub_1_msg_0;
	ret = pub_sub_handle_queued_msg(subscriber_1, K_NO_WAIT);
	zassert_ok(ret);
	ret = pub_sub_handle_queued_msg(subscriber_1, K_MSEC(5));
	zassert_ok(ret);
}

ZTEST(delayable_msg, test_abort_queued_msg)
{
	struct pub_sub_subscriber *subscriber_0 = &g_test_subscriber_0->subscriber;
	int ret;

	// Start a message and let it expire
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(100));

	// Aborting the message should return an error
	zassert_not_ok(pub_sub_delayable_msg_abort(g_sub_0_msg_0));

	// The message should be received
	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_0;
	g_sub_0_expected_msg.msg = g_sub_0_msg_0;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_NO_WAIT);
	zassert_ok(ret);

	// Start a message and let it expire twice
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(100));
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(100));

	// Aborting the message should return an error
	zassert_not_ok(pub_sub_delayable_msg_abort(g_sub_0_msg_0));

	// The message should be received once
	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_0;
	g_sub_0_expected_msg.msg = g_sub_0_msg_0;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_NO_WAIT);
	zassert_ok(ret);
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(200));
	zassert_not_ok(ret);
}

ZTEST(delayable_msg, test_update_single_msg)
{
	struct pub_sub_subscriber *subscriber_0 = &g_test_subscriber_0->subscriber;
	int ret;
	int64_t start_ms;

	// Start a message and pass some time
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(50));

	// Update the timeout to be later
	zassert_ok(pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(200)));
	start_ms = k_uptime_get();

	// The message should be received at the correct time
	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_0;
	g_sub_0_expected_msg.msg = g_sub_0_msg_0;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(250));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 200);

	// Start a message and pass some time
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(200));
	k_sleep(K_MSEC(50));

	// Update the timeout to be earlier
	zassert_ok(pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100)));
	start_ms = k_uptime_get();

	// The message should be received at the correct time
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(250));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 100);
}

ZTEST(delayable_msg, test_update_multi_msg)
{
	struct pub_sub_subscriber *subscriber_0 = &g_test_subscriber_0->subscriber;
	int ret;
	int64_t start_ms;

	// Start three messages and pass some time
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(150));
	pub_sub_delayable_msg_start(g_sub_0_msg_1, K_MSEC(250));
	pub_sub_delayable_msg_start(g_sub_0_msg_2, K_MSEC(350));
	k_sleep(K_MSEC(50));

	// Update the message timeouts
	zassert_ok(pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(250)));
	zassert_ok(pub_sub_delayable_msg_start(g_sub_0_msg_1, K_MSEC(150)));
	zassert_ok(pub_sub_delayable_msg_start(g_sub_0_msg_2, K_MSEC(50)));
	start_ms = k_uptime_get();

	// The messages should be received at the correct time
	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_2;
	g_sub_0_expected_msg.msg = g_sub_0_msg_2;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(250));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 50);

	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_1;
	g_sub_0_expected_msg.msg = g_sub_0_msg_1;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(250));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 150);

	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_0;
	g_sub_0_expected_msg.msg = g_sub_0_msg_0;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(250));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 250);
}

ZTEST(delayable_msg, test_update_single_msg_with_expired)
{
	struct pub_sub_subscriber *subscriber_0 = &g_test_subscriber_0->subscriber;
	int ret;
	int64_t start_ms;

	// Start a message and let it expire then start it again and let it expire
	pub_sub_delayable_msg_start(g_sub_1_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(150));
	pub_sub_delayable_msg_start(g_sub_1_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(150));

	// Start a message and pass some time
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(50));

	// Update the timeout to be later
	zassert_ok(pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(200)));
	start_ms = k_uptime_get();

	// The message should be received at the correct time
	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_0;
	g_sub_0_expected_msg.msg = g_sub_0_msg_0;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(250));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 200);

	// Start a message and pass some time
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(200));
	k_sleep(K_MSEC(50));

	// Update the timeout to be earlier
	zassert_ok(pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100)));
	start_ms = k_uptime_get();

	// The message should be received at the correct time
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(250));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 100);
}

ZTEST(delayable_msg, test_update_multi_msg_with_expired)
{
	struct pub_sub_subscriber *subscriber_0 = &g_test_subscriber_0->subscriber;
	int ret;
	int64_t start_ms;

	// Start a message and let it expire then start it again and let it expire
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
	zassert_ok(pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(250)));
	zassert_ok(pub_sub_delayable_msg_start(g_sub_0_msg_1, K_MSEC(150)));
	zassert_ok(pub_sub_delayable_msg_start(g_sub_0_msg_2, K_MSEC(50)));
	start_ms = k_uptime_get();

	// The messages should be received at the correct time
	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_2;
	g_sub_0_expected_msg.msg = g_sub_0_msg_2;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(250));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 50);

	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_1;
	g_sub_0_expected_msg.msg = g_sub_0_msg_1;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(250));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 150);

	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_0;
	g_sub_0_expected_msg.msg = g_sub_0_msg_0;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(250));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 250);
}

ZTEST(delayable_msg, test_update_queued_msg)
{
	struct pub_sub_subscriber *subscriber_0 = &g_test_subscriber_0->subscriber;
	int ret;
	int64_t start_ms;

	// Start a message and let it expire
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(100));

	// Update timeout without handling the message, should return error
	zassert_not_ok(pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(200)));
	start_ms = k_uptime_get();

	// The queued message should be received immediately
	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_0;
	g_sub_0_expected_msg.msg = g_sub_0_msg_0;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_NO_WAIT);
	zassert_ok(ret);

	// The message should also be received after the updated timeout
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(250));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 200);

	// Start the message and let it expire twice
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(100));
	pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(100));
	k_sleep(K_MSEC(100));

	// Update timeout without handling the message, should return error
	zassert_not_ok(pub_sub_delayable_msg_start(g_sub_0_msg_0, K_MSEC(200)));
	start_ms = k_uptime_get();

	// The queued message should be received immediately
	g_sub_0_expected_msg.msg_id = MSG_ID_TIMER_0;
	g_sub_0_expected_msg.msg = g_sub_0_msg_0;
	ret = pub_sub_handle_queued_msg(subscriber_0, K_NO_WAIT);
	zassert_ok(ret);

	// The message should also be received after the updated timeout
	ret = pub_sub_handle_queued_msg(subscriber_0, K_MSEC(250));
	zassert_ok(ret);
	zassert_equal(k_uptime_get() - start_ms, 200);
}

ZTEST_SUITE(delayable_msg, NULL, delayable_msg_suite_setup, delayable_msg_before_test,
	    delayable_msg_after_test, delayable_msg_suite_teardown);