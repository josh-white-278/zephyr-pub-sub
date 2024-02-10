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
};

struct static_msg {
	uint32_t test_data;
};

PUB_SUB_STATIC_DELAYABLE_MSG_DEFINE(struct static_msg, g_delayable_msg, MSG_ID_TIMER_0, NULL);

static void delayable_msg_before_test(void *fixture)
{
	reset_default_broker();
}

static void delayable_msg_after_test(void *fixture)
{
	ARG_UNUSED(fixture);
	(void)pub_sub_delayable_msg_abort(g_delayable_msg);
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

ZTEST(delayable_msg, test_delayable_msg)
{
	struct fifo_subscriber *f_subscriber = malloc_fifo_subscriber(MSG_ID_MAX_PUB_ID);
	struct pub_sub_subscriber *subscriber = &f_subscriber->subscriber;
	struct msg_handler_data handler_data = {.msg_id = MSG_ID_TIMER_0, .msg = g_delayable_msg};
	int ret;

	// Subscriber is dynamically allocated so we need to re-initialize
	pub_sub_delayable_msg_init(g_delayable_msg, subscriber, MSG_ID_TIMER_0);

	// Basic test that a subscriber receives a published delayable message
	pub_sub_subscriber_set_handler_data(subscriber, msg_handler, &handler_data);
	pub_sub_add_subscriber(subscriber);

	g_delayable_msg->test_data = 12345;
	handler_data.data_value = 12345;
	pub_sub_delayable_msg_start(g_delayable_msg, K_MSEC(500));

	// There should be no message until the delayable message timeout expires
	ret = pub_sub_handle_queued_msg(subscriber, K_MSEC(499));
	zassert_not_ok(ret);
	zassert_equal(pub_sub_msg_get_ref_cnt(g_delayable_msg), 0);

	// The delayable message should then be received
	ret = pub_sub_handle_queued_msg(subscriber, K_MSEC(1));
	zassert_ok(ret);
	zassert_equal(pub_sub_msg_get_ref_cnt(g_delayable_msg), 0);

	// Test msg can be re-scheduled
	g_delayable_msg->test_data = 54321;
	handler_data.data_value = 54321;
	pub_sub_delayable_msg_start(g_delayable_msg, K_SECONDS(2));

	// There should be no message until the delayable message timeout expires
	ret = pub_sub_handle_queued_msg(subscriber, K_MSEC(1999));
	zassert_not_ok(ret);
	zassert_equal(pub_sub_msg_get_ref_cnt(g_delayable_msg), 0);

	// The delayable message should then be received
	ret = pub_sub_handle_queued_msg(subscriber, K_MSEC(1));
	zassert_ok(ret);
	zassert_equal(pub_sub_msg_get_ref_cnt(g_delayable_msg), 0);
}

ZTEST(delayable_msg, test_delayable_abort)
{
	struct fifo_subscriber *f_subscriber = malloc_fifo_subscriber(MSG_ID_MAX_PUB_ID);
	struct pub_sub_subscriber *subscriber = &f_subscriber->subscriber;
	struct msg_handler_data handler_data = {.msg_id = MSG_ID_TIMER_0, .msg = g_delayable_msg};
	int ret;

	// Subscriber is dynamically allocated so we need to re-initialize
	pub_sub_delayable_msg_init(g_delayable_msg, subscriber, MSG_ID_TIMER_0);

	pub_sub_subscriber_set_handler_data(subscriber, msg_handler, &handler_data);
	pub_sub_add_subscriber(subscriber);

	pub_sub_delayable_msg_start(g_delayable_msg, K_MSEC(500));

	// Let the message almost expire
	ret = pub_sub_handle_queued_msg(subscriber, K_MSEC(499));
	zassert_not_ok(ret);

	// Abort the delayable message, should return ok because it should
	// still be scheduled when abort is called
	ret = pub_sub_delayable_msg_abort(g_delayable_msg);
	zassert_ok(ret);

	// The message should not be published
	ret = pub_sub_handle_queued_msg(subscriber, K_MSEC(100));
	zassert_not_ok(ret);

	// Re-scheduled the message
	pub_sub_delayable_msg_start(g_delayable_msg, K_MSEC(500));

	// Delay to let the message get published but don't handle it yet
	k_sleep(K_MSEC(500));

	// Abort the delayable message, should return not ok because it has already been published
	// but has not been handled yet
	ret = pub_sub_delayable_msg_abort(g_delayable_msg);
	zassert_not_ok(ret);

	// The delayable message should then be received
	ret = pub_sub_handle_queued_msg(subscriber, K_NO_WAIT);
	zassert_ok(ret);
	zassert_equal(pub_sub_msg_get_ref_cnt(g_delayable_msg), 0);

	// Aborting the delayable message when it is not started should return ok
	ret = pub_sub_delayable_msg_abort(g_delayable_msg);
	zassert_ok(ret);
}

ZTEST(delayable_msg, test_delayable_update)
{
	struct fifo_subscriber *f_subscriber = malloc_fifo_subscriber(MSG_ID_MAX_PUB_ID);
	struct pub_sub_subscriber *subscriber = &f_subscriber->subscriber;
	struct msg_handler_data handler_data = {.msg_id = MSG_ID_TIMER_0, .msg = g_delayable_msg};
	int ret;

	// Subscriber is dynamically allocated so we need to re-initialize
	pub_sub_delayable_msg_init(g_delayable_msg, subscriber, MSG_ID_TIMER_0);

	pub_sub_subscriber_set_handler_data(subscriber, msg_handler, &handler_data);
	pub_sub_add_subscriber(subscriber);

	g_delayable_msg->test_data = 234;
	handler_data.data_value = 234;

	// Updating the delayable message should return ok when it has never been started
	ret = pub_sub_delayable_msg_update_timeout(g_delayable_msg, K_MSEC(500));
	zassert_ok(ret);

	// Let the message almost expire
	ret = pub_sub_handle_queued_msg(subscriber, K_MSEC(499));
	zassert_not_ok(ret);

	// Updating the delayable message should return ok when it is still scheduled
	ret = pub_sub_delayable_msg_update_timeout(g_delayable_msg, K_MSEC(500));
	zassert_ok(ret);

	// The message should not be published
	ret = pub_sub_handle_queued_msg(subscriber, K_MSEC(100));
	zassert_not_ok(ret);

	// Delay to let the message get published but don't handle it yet
	k_sleep(K_MSEC(500));

	//  Updating the delayable message should return not ok because it has already been
	//  published but has not been handled yet
	ret = pub_sub_delayable_msg_update_timeout(g_delayable_msg, K_MSEC(1000));
	zassert_not_ok(ret);

	// The delayable message should then be received from the first publish
	ret = pub_sub_handle_queued_msg(subscriber, K_NO_WAIT);
	zassert_ok(ret);
	zassert_equal(pub_sub_msg_get_ref_cnt(g_delayable_msg), 0);

	// The delayable message should then be received after the updated amount of time has passed
	ret = pub_sub_handle_queued_msg(subscriber, K_MSEC(999));
	zassert_not_ok(ret);
	ret = pub_sub_handle_queued_msg(subscriber, K_MSEC(1));
	zassert_ok(ret);
	zassert_equal(pub_sub_msg_get_ref_cnt(g_delayable_msg), 0);
}

static void msg_restart_handler(uint16_t msg_id, const void *msg, void *user_data)
{
	struct msg_handler_data *data = user_data;
	const struct static_msg *static_msg = msg;
	zassert_equal(msg_id, data->msg_id);
	zassert_equal(static_msg->test_data, data->data_value);
	zassert_equal_ptr(msg, data->msg);
	pub_sub_delayable_msg_start(msg, K_MSEC(500));
}

ZTEST(delayable_msg, test_delayable_msg_handler_restart)
{
	struct fifo_subscriber *f_subscriber = malloc_fifo_subscriber(MSG_ID_MAX_PUB_ID);
	struct pub_sub_subscriber *subscriber = &f_subscriber->subscriber;
	struct msg_handler_data handler_data = {.msg_id = MSG_ID_TIMER_0, .msg = g_delayable_msg};
	int ret;

	// Subscriber is dynamically allocated so we need to re-initialize
	pub_sub_delayable_msg_init(g_delayable_msg, subscriber, MSG_ID_TIMER_0);

	// Add the restart handler as the message handler
	pub_sub_subscriber_set_handler_data(subscriber, msg_restart_handler, &handler_data);
	pub_sub_add_subscriber(subscriber);

	g_delayable_msg->test_data = 12345;
	handler_data.data_value = 12345;
	pub_sub_delayable_msg_start(g_delayable_msg, K_MSEC(500));

	// Test continuously restarting a delayable message
	for (int i = 0; i < 10; i++) {
		// There should be no message until the delayable message timeout expires
		ret = pub_sub_handle_queued_msg(subscriber, K_MSEC(499));
		zassert_not_ok(ret);

		// The delayable message should then be received
		ret = pub_sub_handle_queued_msg(subscriber, K_MSEC(1));
		zassert_ok(ret);
	}
}

ZTEST_SUITE(delayable_msg, NULL, NULL, delayable_msg_before_test, delayable_msg_after_test, NULL);