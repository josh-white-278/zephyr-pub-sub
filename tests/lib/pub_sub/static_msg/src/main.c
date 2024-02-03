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

struct msg_handler_data {
	uint16_t msg_id;
	uint32_t data_value;
	void *msg;
};

static void msg_handler(uint16_t msg_id, const void *msg, void *user_data)
{
	struct msg_handler_data *data = user_data;
	const struct static_msg *static_msg = msg;
	zassert_equal(msg_id, data->msg_id);
	zassert_equal(static_msg->test_data, data->data_value);
	zassert_equal_ptr(msg, data->msg);
}

ZTEST(static_msg, test_static_msg)
{
	struct fifo_subscriber *f_subscriber = malloc_fifo_subscriber(MSG_ID_MAX_PUB_ID);
	struct pub_sub_subscriber *subscriber = &f_subscriber->subscriber;
	struct msg_handler_data handler_data = {.msg_id = MSG_ID_SUBSCRIBED_ID_0,
						.msg = g_static_msg};
	int ret;

	// Basic test that a subscriber receives a published static message
	pub_sub_subscriber_set_handler_data(subscriber, msg_handler, &handler_data);
	pub_sub_add_subscriber(subscriber);
	pub_sub_subscribe(subscriber, MSG_ID_SUBSCRIBED_ID_0);

	g_static_msg->test_data = 12345;
	handler_data.data_value = 12345;
	pub_sub_acquire_msg(g_static_msg);
	pub_sub_publish(g_static_msg);

	// Needs a small delay to allow the worker thread to run
	ret = pub_sub_handle_queued_msg(subscriber, K_MSEC(1));
	zassert_ok(ret);
	zassert_equal(pub_sub_msg_get_ref_cnt(g_static_msg), 0);

	// Test msg can be re-published
	g_static_msg->test_data = 54321;
	handler_data.data_value = 54321;
	pub_sub_acquire_msg(g_static_msg);
	pub_sub_publish(g_static_msg);

	// Needs a small delay to allow the worker thread to run
	ret = pub_sub_handle_queued_msg(subscriber, K_MSEC(1));
	zassert_ok(ret);
	zassert_equal(pub_sub_msg_get_ref_cnt(g_static_msg), 0);
}

ZTEST(static_msg, test_callback_msg)
{
	struct fifo_subscriber *f_subscribers[4] = {};
	struct msg_handler_data handler_data = {.msg_id = MSG_ID_SUBSCRIBED_ID_0,
						.msg = g_callback_msg};
	int ret;

	// Create 4 subscribers and subscribe to the callback msg
	for (size_t i = 0; i < ARRAY_SIZE(f_subscribers); i++) {
		f_subscribers[i] = malloc_fifo_subscriber(MSG_ID_MAX_PUB_ID);
		struct pub_sub_subscriber *subscriber = &f_subscribers[i]->subscriber;
		pub_sub_subscriber_set_handler_data(subscriber, msg_handler, &handler_data);
		pub_sub_add_subscriber(subscriber);
		pub_sub_subscribe(subscriber, MSG_ID_SUBSCRIBED_ID_0);
	}

	g_callback_msg->test_data = 12345;
	handler_data.data_value = 12345;
	pub_sub_acquire_msg(g_callback_msg);
	pub_sub_publish(g_callback_msg);

	// Each subscriber should receive the callback msg
	for (size_t i = 0; i < ARRAY_SIZE(f_subscribers); i++) {
		// Needs a small delay to allow the worker thread to run
		ret = pub_sub_handle_queued_msg(&f_subscribers[i]->subscriber, K_MSEC(1));
		zassert_ok(ret);
	}

	// After the ref cnt hits zero the msg callback should be called once
	void *callback_msg;
	ret = k_msgq_get(&g_callback_msgq, &callback_msg, K_NO_WAIT);
	zassert_ok(ret);
	zassert_equal_ptr(g_callback_msg, callback_msg);
	zassert_not_ok(k_msgq_get(&g_callback_msgq, &callback_msg, K_NO_WAIT));
	// Reference counter should be reset back to 0 after callback
	zassert_equal(pub_sub_msg_get_ref_cnt(g_callback_msg), 0);

	// Test that the msg can be re-published after the callback has been called
	g_callback_msg->test_data = 54321;
	handler_data.data_value = 54321;
	pub_sub_acquire_msg(g_callback_msg);
	pub_sub_publish(g_callback_msg);

	// Each subscriber should receive the callback msg
	for (size_t i = 0; i < ARRAY_SIZE(f_subscribers); i++) {
		// Needs a small delay to allow the worker thread to run
		ret = pub_sub_handle_queued_msg(&f_subscribers[i]->subscriber, K_MSEC(1));
		zassert_ok(ret);
	}

	// After the ref cnt hits zero the msg callback should be called once
	ret = k_msgq_get(&g_callback_msgq, &callback_msg, K_NO_WAIT);
	zassert_ok(ret);
	zassert_equal_ptr(g_callback_msg, callback_msg);
	zassert_not_ok(k_msgq_get(&g_callback_msgq, &callback_msg, K_NO_WAIT));
	// Reference counter should be reset back to 0 after callback
	zassert_equal(pub_sub_msg_get_ref_cnt(g_callback_msg), 0);
}

ZTEST_SUITE(static_msg, NULL, NULL, static_msg_before_test, NULL, NULL);