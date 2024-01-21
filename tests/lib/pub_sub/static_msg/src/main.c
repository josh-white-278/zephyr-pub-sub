/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <pub_sub/pub_sub.h>
#include <pub_sub/static_msg.h>
#include <zephyr/ztest.h>
#include <stdlib.h>
#include <helpers.h>

enum msg_id {
	MSG_ID_SUBSCRIBED_ID_0,
	MSG_ID_MAX_PUB_ID = MSG_ID_SUBSCRIBED_ID_0,
};

struct static_msg {
	uint32_t test_data;
};

// Callback msgq so we can test the callback is called when the msg is freed
K_MSGQ_DEFINE(g_callback_msgq, sizeof(void *), 8, sizeof(void *));
static void msg_callback(const void *msg)
{
	int ret = k_msgq_put(&g_callback_msgq, &msg, K_NO_WAIT);
	zassert_ok(ret);
}

PUB_SUB_STATIC_MSG_DEFINE(struct static_msg, g_static_msg, MSG_ID_SUBSCRIBED_ID_0);
PUB_SUB_STATIC_CALLBACK_MSG_DEFINE(struct static_msg, g_callback_msg, MSG_ID_SUBSCRIBED_ID_0,
				   msg_callback);

static void static_msg_before_test(void *fixture)
{
	reset_default_broker();
}

ZTEST(static_msg, test_static_msg)
{
	struct callback_subscriber *c_subscriber = malloc_callback_subscriber(MSG_ID_MAX_PUB_ID);
	struct pub_sub_subscriber *subscriber = &c_subscriber->subscriber;
	struct rx_msg rx_msg;
	int ret;

	// Basic test that a subscriber receives a published static message
	pub_sub_add_subscriber(subscriber);
	pub_sub_subscribe(subscriber, MSG_ID_SUBSCRIBED_ID_0);

	g_static_msg->test_data = 12345;
	pub_sub_publish(g_static_msg);

	ret = k_msgq_get(&c_subscriber->msgq, &rx_msg,
			 K_MSEC(1)); // Needs a small delay to allow the worker thread to run
	zassert_ok(ret);
	zassert_equal(MSG_ID_SUBSCRIBED_ID_0, rx_msg.msg_id);
	zassert_equal_ptr(g_static_msg, rx_msg.msg);
	const struct static_msg *rx_msg_ptr = rx_msg.msg;
	zassert_equal(rx_msg_ptr->test_data, 12345);
	pub_sub_release_msg(rx_msg.msg);
	zassert_equal(pub_sub_msg_get_ref_cnt(g_static_msg), 0);

	// Test msg can be re-published after being reset
	pub_sub_static_msg_reset(g_static_msg);
	g_static_msg->test_data = 54321;
	pub_sub_publish(g_static_msg);

	ret = k_msgq_get(&c_subscriber->msgq, &rx_msg,
			 K_MSEC(1)); // Needs a small delay to allow the worker thread to run
	zassert_ok(ret);
	zassert_equal(MSG_ID_SUBSCRIBED_ID_0, rx_msg.msg_id);
	zassert_equal_ptr(g_static_msg, rx_msg.msg);
	rx_msg_ptr = rx_msg.msg;
	zassert_equal(rx_msg_ptr->test_data, 54321);
	pub_sub_release_msg(rx_msg.msg);
	zassert_equal(pub_sub_msg_get_ref_cnt(g_static_msg), 0);
}

ZTEST(static_msg, test_callback_msg)
{
	struct callback_subscriber *c_subscribers[4] = {};
	struct rx_msg rx_msg;
	int ret;

	// Create 4 subscribers and subscribe to the callback msg
	for (size_t i = 0; i < ARRAY_SIZE(c_subscribers); i++) {
		c_subscribers[i] = malloc_callback_subscriber(MSG_ID_MAX_PUB_ID);
		struct pub_sub_subscriber *subscriber = &c_subscribers[i]->subscriber;
		pub_sub_add_subscriber(subscriber);
		pub_sub_subscribe(subscriber, MSG_ID_SUBSCRIBED_ID_0);
	}

	g_callback_msg->test_data = 12345;
	pub_sub_publish(g_callback_msg);

	// Each subscriber should receive the callback msg
	for (size_t i = 0; i < ARRAY_SIZE(c_subscribers); i++) {
		ret = k_msgq_get(
			&c_subscribers[i]->msgq, &rx_msg,
			K_MSEC(1)); // Needs a small delay to allow the worker thread to run
		zassert_ok(ret);
		zassert_equal(MSG_ID_SUBSCRIBED_ID_0, rx_msg.msg_id);
		zassert_equal_ptr(g_callback_msg, rx_msg.msg);
		const struct static_msg *rx_msg_ptr = rx_msg.msg;
		zassert_equal(rx_msg_ptr->test_data, 12345);
		pub_sub_release_msg(rx_msg.msg);
	}

	// After the ref cnt hits zero the msg callback should be called once
	void *callback_msg;
	ret = k_msgq_get(&g_callback_msgq, &callback_msg, K_NO_WAIT);
	zassert_ok(ret);
	zassert_equal_ptr(g_callback_msg, callback_msg);
	zassert_not_ok(k_msgq_get(&g_callback_msgq, &callback_msg, K_NO_WAIT));
	// Reference counter should be reset back to 1 after callback
	zassert_equal(pub_sub_msg_get_ref_cnt(g_callback_msg), 1);

	// Test that the msg can be re-published after the callback has been called
	g_callback_msg->test_data = 54321;
	pub_sub_publish(g_callback_msg);

	// Each subscriber should receive the callback msg
	for (size_t i = 0; i < ARRAY_SIZE(c_subscribers); i++) {
		ret = k_msgq_get(
			&c_subscribers[i]->msgq, &rx_msg,
			K_MSEC(1)); // Needs a small delay to allow the worker thread to run
		zassert_ok(ret);
		zassert_equal(MSG_ID_SUBSCRIBED_ID_0, rx_msg.msg_id);
		zassert_equal_ptr(g_callback_msg, rx_msg.msg);
		const struct static_msg *rx_msg_ptr = rx_msg.msg;
		zassert_equal(rx_msg_ptr->test_data, 54321);
		pub_sub_release_msg(rx_msg.msg);
	}

	// After the ref cnt hits zero the msg callback should be called once
	ret = k_msgq_get(&g_callback_msgq, &callback_msg, K_NO_WAIT);
	zassert_ok(ret);
	zassert_equal_ptr(g_callback_msg, callback_msg);
	zassert_not_ok(k_msgq_get(&g_callback_msgq, &callback_msg, K_NO_WAIT));
	// Reference counter should be reset back to 1 after callback
	zassert_equal(pub_sub_msg_get_ref_cnt(g_callback_msg), 1);
}

ZTEST_SUITE(static_msg, NULL, NULL, static_msg_before_test, NULL, NULL);