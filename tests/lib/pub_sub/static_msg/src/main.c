/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <pub_sub/pub_sub.h>
#include <pub_sub/static_msg.h>
#include <zephyr/ztest.h>
#include <stdlib.h>

enum msg_id {
	MSG_ID_SUBSCRIBED_ID_0,
	MSG_ID_MAX_PUB_ID = MSG_ID_SUBSCRIBED_ID_0,
};

struct static_msg {
	uint32_t test_data;
};

struct rx_msg {
	uint16_t msg_id;
	uint32_t data_value;
	const void *msg_ptr;
};

struct test_subscriber {
	PUB_SUB_SUBSCRIBER_COMPOSE(MSG_ID_MAX_PUB_ID);
	struct k_msgq *rx_msgq;
};

static void msg_handler(struct pub_sub_subscriber *subscriber, uint16_t msg_id, const void *msg);

// Callback msgq so we can test the callback is called when the msg is freed
K_MSGQ_DEFINE(g_callback_msgq, sizeof(void *), 8, sizeof(void *));
static void msg_callback(const void *msg)
{
	int ret = k_msgq_put(&g_callback_msgq, &msg, K_NO_WAIT);
	zassert_ok(ret);
}

K_MSGQ_DEFINE(g_rx_msg_queue, sizeof(struct rx_msg), 32, 1);
PUB_SUB_STATIC_MSG_DEFINE(struct static_msg, g_static_msg, MSG_ID_SUBSCRIBED_ID_0);
PUB_SUB_STATIC_CALLBACK_MSG_DEFINE(struct static_msg, g_callback_msg, MSG_ID_SUBSCRIBED_ID_0,
				   msg_callback);
static struct test_subscriber g_test_subscriber = {
	PUB_SUB_SUBSCRIBER_INIT_COMPOSED(g_test_subscriber, &k_sys_work_q, msg_handler,
					 MSG_ID_MAX_PUB_ID, 0),
	.rx_msgq = &g_rx_msg_queue,
};

static void msg_handler(struct pub_sub_subscriber *subscriber, uint16_t msg_id, const void *msg)
{
	struct test_subscriber *test_subscriber =
		PUB_SUB_CONTAINER_FROM_SUBSCRIBER(subscriber, struct test_subscriber);
	const struct static_msg *static_msg = msg;
	struct rx_msg rx_msg = {
		.msg_id = msg_id,
		.data_value = static_msg->test_data,
		.msg_ptr = msg,
	};
	k_msgq_put(test_subscriber->rx_msgq, &rx_msg, K_FOREVER);
}

static void *suite_setup(void)
{
	pub_sub_add_subscriber(PUB_SUB_COMPOSED_SUBSCRIBER_PTR(&g_test_subscriber));
	pub_sub_subscribe(PUB_SUB_COMPOSED_SUBSCRIBER_PTR(&g_test_subscriber),
			  MSG_ID_SUBSCRIBED_ID_0);
	return NULL;
}

ZTEST(static_msg, test_static_msg)
{
	struct rx_msg rx_msg;

	g_static_msg->test_data = 12345;

	pub_sub_acquire_msg(g_static_msg);
	pub_sub_publish(g_static_msg);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(10)));
	zassert_equal(pub_sub_msg_get_ref_cnt(g_static_msg), 0);
	zassert_equal(rx_msg.msg_id, MSG_ID_SUBSCRIBED_ID_0);
	zassert_equal(rx_msg.data_value, 12345);
	zassert_equal_ptr(rx_msg.msg_ptr, g_static_msg);

	// Test msg can be re-published
	g_static_msg->test_data = 54321;
	pub_sub_acquire_msg(g_static_msg);
	pub_sub_publish(g_static_msg);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(10)));
	zassert_equal(pub_sub_msg_get_ref_cnt(g_static_msg), 0);
	zassert_equal(rx_msg.msg_id, MSG_ID_SUBSCRIBED_ID_0);
	zassert_equal(rx_msg.data_value, 54321);
	zassert_equal_ptr(rx_msg.msg_ptr, g_static_msg);
}

ZTEST(static_msg, test_callback_msg)
{
	struct rx_msg rx_msg;
	void *callback_msg;

	g_callback_msg->test_data = 23456;

	// Aquire 4 references to the callback message and publish and receive the message 4 times
	// the message's callback function should only be called after the 4th publish i.e. the
	// final reference is released
	for (size_t i = 0; i < 4; i++) {
		pub_sub_acquire_msg(g_callback_msg);
	}

	for (size_t i = 0; i < 4; i++) {
		// The message's callback should not be called until after the 4th publish/receive
		zassert_not_ok(k_msgq_get(&g_callback_msgq, &callback_msg, K_MSEC(10)));

		pub_sub_publish(g_callback_msg);

		zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(10)));
		zassert_equal(rx_msg.msg_id, MSG_ID_SUBSCRIBED_ID_0);
		zassert_equal(rx_msg.data_value, 23456);
		zassert_equal_ptr(rx_msg.msg_ptr, g_callback_msg);
	}

	// After the ref cnt hits zero the msg callback should be called once
	zassert_ok(k_msgq_get(&g_callback_msgq, &callback_msg, K_NO_WAIT));
	zassert_equal_ptr(g_callback_msg, callback_msg);
	// Reference counter should be reset back to 0 after callback
	zassert_equal(pub_sub_msg_get_ref_cnt(g_callback_msg), 0);
	zassert_not_ok(k_msgq_get(&g_callback_msgq, &callback_msg, K_NO_WAIT));

	// Test that the msg can be re-published after the callback has been called
	g_callback_msg->test_data = 65432;
	pub_sub_acquire_msg(g_callback_msg);
	pub_sub_publish(g_callback_msg);

	zassert_ok(k_msgq_get(&g_rx_msg_queue, &rx_msg, K_MSEC(10)));
	zassert_equal(rx_msg.msg_id, MSG_ID_SUBSCRIBED_ID_0);
	zassert_equal(rx_msg.data_value, 65432);
	zassert_equal_ptr(rx_msg.msg_ptr, g_callback_msg);

	// After the ref cnt hits zero the msg callback should be called once
	zassert_ok(k_msgq_get(&g_callback_msgq, &callback_msg, K_NO_WAIT));
	zassert_equal_ptr(g_callback_msg, callback_msg);
	zassert_not_ok(k_msgq_get(&g_callback_msgq, &callback_msg, K_NO_WAIT));
	// Reference counter should be reset back to 0 after callback
	zassert_equal(pub_sub_msg_get_ref_cnt(g_callback_msg), 0);
}

ZTEST_SUITE(static_msg, NULL, suite_setup, NULL, NULL, NULL);