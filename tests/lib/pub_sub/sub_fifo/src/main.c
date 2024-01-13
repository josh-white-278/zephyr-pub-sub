/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <pub_sub/pub_sub.h>
#include <pub_sub/msg_alloc_mem_slab.h>
#include <zephyr/ztest.h>
#include <stdlib.h>
#include <helpers.h>

#define TEST_MSG_SIZE_BYTES 8

enum msg_id {
	MSG_ID_SUBSCRIBED_ID_0,
	MSG_ID_NOT_SUBSCRIBED_ID_0,
	MSG_ID_SUBSCRIBED_ID_1,
	MSG_ID_NOT_SUBSCRIBED_ID_1,
	MSG_ID_SUBSCRIBED_ID_2,
	MSG_ID_NOT_SUBSCRIBED_ID_2,
	MSG_ID_SUBSCRIBED_ID_3,
	MSG_ID_NOT_SUBSCRIBED_ID_3,
	MSG_ID_NUM_IDS,
};

PUB_SUB_MEM_SLAB_ALLOCATOR_DEFINE_STATIC(test_allocator, TEST_MSG_SIZE_BYTES, 32);

static void fifo_before_test(void *fixture)
{
	ARG_UNUSED(fixture);
	reset_default_broker();
}

static void fifo_after_test(void *fixture)
{
	ARG_UNUSED(fixture);
	// Check for leaked messages
	struct k_mem_slab *mem_slab = test_allocator.impl;
	__ASSERT(k_mem_slab_num_used_get(mem_slab) == 0, "");
}

struct msg_handler_data {
	uint16_t msg_id;
	void *msg;
};

static void msg_handler(uint16_t msg_id, const void *msg, void *user_data)
{
	struct msg_handler_data *data = user_data;
	zassert_equal(msg_id, data->msg_id);
	if (data->msg != NULL) {
		zassert_equal_ptr(msg, data->msg);
	}
}

ZTEST(fifo, test_add_remove_subscriber)
{
	struct pub_sub_allocator *allocator = &test_allocator;
	struct fifo_subscriber *f_subscriber = malloc_fifo_subscriber(MSG_ID_NUM_IDS);
	struct pub_sub_subscriber *subscriber = &f_subscriber->subscriber;
	struct msg_handler_data handler_data = {.msg_id = MSG_ID_SUBSCRIBED_ID_0};
	void *msg;
	int ret;

	// Basic test that a subscriber receives a published message
	pub_sub_subscriber_set_handler_data(subscriber, msg_handler, &handler_data);
	pub_sub_add_subscriber(subscriber);
	pub_sub_subscribe(subscriber, MSG_ID_SUBSCRIBED_ID_0);

	msg = pub_sub_new_msg(allocator, MSG_ID_SUBSCRIBED_ID_0, TEST_MSG_SIZE_BYTES, K_NO_WAIT);
	zassert_not_null(msg);
	pub_sub_publish(msg);

	handler_data.msg = msg;
	// Needs a small delay to allow the worker thread to run
	ret = pub_sub_handle_queued_msg(subscriber, K_MSEC(1));
	zassert_ok(ret);

	// Test that a removed subscriber stops receiving messages
	pub_sub_subscriber_remove_broker(subscriber);

	msg = pub_sub_new_msg(allocator, MSG_ID_SUBSCRIBED_ID_0, TEST_MSG_SIZE_BYTES, K_NO_WAIT);
	zassert_not_null(msg);
	pub_sub_publish(msg);

	handler_data.msg = NULL;
	// Needs a small delay to allow the worker thread to run
	ret = pub_sub_handle_queued_msg(subscriber, K_MSEC(1));
	zassert_not_ok(ret);

	// Test that a subscriber maintains its subscriptions and can just be
	// re-added to start receiving msgs again
	pub_sub_add_subscriber(subscriber);

	msg = pub_sub_new_msg(allocator, MSG_ID_SUBSCRIBED_ID_0, TEST_MSG_SIZE_BYTES, K_NO_WAIT);
	zassert_not_null(msg);
	pub_sub_publish(msg);

	handler_data.msg = msg;
	// Needs a small delay to allow the worker thread to run
	ret = pub_sub_handle_queued_msg(subscriber, K_MSEC(1));
	zassert_ok(ret);
}

ZTEST(fifo, test_subscribing)
{
	struct pub_sub_allocator *allocator = &test_allocator;
	struct fifo_subscriber *f_subscriber = malloc_fifo_subscriber(MSG_ID_NUM_IDS);
	struct pub_sub_subscriber *subscriber = &f_subscriber->subscriber;
	struct msg_handler_data handler_data = {};
	void *msg;
	int ret;

	// Test that subscribed msg ids are received and others are not
	pub_sub_subscriber_set_handler_data(subscriber, msg_handler, &handler_data);
	pub_sub_add_subscriber(subscriber);
	pub_sub_subscribe(subscriber, MSG_ID_SUBSCRIBED_ID_0);
	pub_sub_subscribe(subscriber, MSG_ID_SUBSCRIBED_ID_1);
	pub_sub_subscribe(subscriber, MSG_ID_SUBSCRIBED_ID_2);
	pub_sub_subscribe(subscriber, MSG_ID_SUBSCRIBED_ID_3);

	uint16_t pub_ids[] = {
		MSG_ID_SUBSCRIBED_ID_0,     MSG_ID_NOT_SUBSCRIBED_ID_0, MSG_ID_SUBSCRIBED_ID_1,
		MSG_ID_NOT_SUBSCRIBED_ID_1, MSG_ID_SUBSCRIBED_ID_2,     MSG_ID_NOT_SUBSCRIBED_ID_2,
		MSG_ID_SUBSCRIBED_ID_3,     MSG_ID_NOT_SUBSCRIBED_ID_3,
	};
	for (size_t i = 0; i < ARRAY_SIZE(pub_ids); i++) {
		msg = pub_sub_new_msg(allocator, pub_ids[i], TEST_MSG_SIZE_BYTES, K_NO_WAIT);
		zassert_not_null(msg);
		pub_sub_publish(msg);
	}

	// Should only receive MSG_ID_SUBSCRIBED_ID_* msgs
	for (size_t i = 0; i < ARRAY_SIZE(pub_ids); i += 2) {
		handler_data.msg_id = pub_ids[i];
		// Needs a small delay to allow the worker thread to run
		ret = pub_sub_handle_queued_msg(subscriber, K_MSEC(1));
		zassert_ok(ret);
	}

	// Unsubscribe from a couple of the IDs so the subscriber
	// should no longer receive those msg ids
	pub_sub_unsubscribe(subscriber, MSG_ID_SUBSCRIBED_ID_1);
	pub_sub_unsubscribe(subscriber, MSG_ID_SUBSCRIBED_ID_3);
	for (size_t i = 0; i < ARRAY_SIZE(pub_ids); i++) {
		msg = pub_sub_new_msg(allocator, pub_ids[i], TEST_MSG_SIZE_BYTES, K_NO_WAIT);
		zassert_not_null(msg);
		pub_sub_publish(msg);
	}
	for (size_t i = 0; i < ARRAY_SIZE(pub_ids); i += 4) {
		handler_data.msg_id = pub_ids[i];
		// Needs a small delay to allow the worker thread to run
		ret = pub_sub_handle_queued_msg(subscriber, K_MSEC(1));
		zassert_ok(ret);
	}
	// No other messages in the queue
	// Needs a small delay to allow the worker thread to run
	ret = pub_sub_handle_queued_msg(subscriber, K_MSEC(1));
	zassert_not_ok(ret);
}

ZTEST(fifo, test_multi_subscriber)
{
	struct pub_sub_allocator *allocator = &test_allocator;
	struct fifo_subscriber *f_subscribers[4] = {};
	struct msg_handler_data handler_data = {};
	void *msg;
	int ret;

	// Create 4 subscribers all subscribed to a unique msg id
	for (size_t i = 0; i < ARRAY_SIZE(f_subscribers); i++) {
		f_subscribers[i] = malloc_fifo_subscriber(MSG_ID_NUM_IDS);
		struct pub_sub_subscriber *subscriber = &f_subscribers[i]->subscriber;
		pub_sub_subscriber_set_handler_data(subscriber, msg_handler, &handler_data);
		pub_sub_add_subscriber(subscriber);
		pub_sub_subscribe(subscriber, MSG_ID_SUBSCRIBED_ID_0 + i * 2);
	}

	// Publish all of the msg ids once
	uint16_t pub_ids[] = {
		MSG_ID_SUBSCRIBED_ID_0,     MSG_ID_NOT_SUBSCRIBED_ID_0, MSG_ID_SUBSCRIBED_ID_1,
		MSG_ID_NOT_SUBSCRIBED_ID_1, MSG_ID_SUBSCRIBED_ID_2,     MSG_ID_NOT_SUBSCRIBED_ID_2,
		MSG_ID_SUBSCRIBED_ID_3,     MSG_ID_NOT_SUBSCRIBED_ID_3,
	};
	for (size_t i = 0; i < ARRAY_SIZE(pub_ids); i++) {
		msg = pub_sub_new_msg(allocator, pub_ids[i], TEST_MSG_SIZE_BYTES, K_NO_WAIT);
		zassert_not_null(msg);
		pub_sub_publish(msg);
	}

	// Each subscriber should receive a single msg with their msg id
	for (size_t i = 0; i < ARRAY_SIZE(f_subscribers); i++) {
		handler_data.msg_id = pub_ids[i * 2];
		// Needs a small delay to allow the worker thread to run
		ret = pub_sub_handle_queued_msg(&f_subscribers[i]->subscriber, K_MSEC(1));
		zassert_ok(ret);

		// No other messages in the queue
		// Needs a small delay to allow the worker thread to run
		ret = pub_sub_handle_queued_msg(&f_subscribers[i]->subscriber, K_MSEC(1));
		zassert_not_ok(ret);
	}

	// Have the subscribers subscribe to all of the messages
	for (size_t i = 0; i < ARRAY_SIZE(f_subscribers); i++) {
		struct pub_sub_subscriber *subscriber = &f_subscribers[i]->subscriber;
		pub_sub_subscribe(subscriber, MSG_ID_SUBSCRIBED_ID_0);
		pub_sub_subscribe(subscriber, MSG_ID_SUBSCRIBED_ID_1);
		pub_sub_subscribe(subscriber, MSG_ID_SUBSCRIBED_ID_2);
		pub_sub_subscribe(subscriber, MSG_ID_SUBSCRIBED_ID_3);
	}

	// Publish all of the messages
	for (size_t i = 0; i < ARRAY_SIZE(pub_ids); i++) {
		msg = pub_sub_new_msg(allocator, pub_ids[i], TEST_MSG_SIZE_BYTES, K_NO_WAIT);
		zassert_not_null(msg);
		pub_sub_publish(msg);
	}

	// Each subscriber should receive each of the subscribed messages once
	// This check relies on the subscribers being in the same order in the
	// broker's subscribers list as in f_subscribers.
	for (size_t i = 0; i < ARRAY_SIZE(f_subscribers); i++) {
		for (uint16_t msg_id = MSG_ID_SUBSCRIBED_ID_0; msg_id <= MSG_ID_SUBSCRIBED_ID_3;
		     msg_id += 2) {
			handler_data.msg_id = msg_id;
			// Needs a small delay to allow the worker thread to run
			ret = pub_sub_handle_queued_msg(&f_subscribers[i]->subscriber, K_MSEC(1));
			zassert_ok(ret);
		}

		// No other messages in the queue
		// Needs a small delay to allow the worker thread to run
		ret = pub_sub_handle_queued_msg(&f_subscribers[i]->subscriber, K_MSEC(1));
		zassert_not_ok(ret);
	}
}

ZTEST(fifo, test_poll_evt)
{
	struct pub_sub_allocator *allocator = &test_allocator;
	const size_t num_msgs = 4;
	struct fifo_subscriber *f_subscriber = malloc_fifo_subscriber(MSG_ID_NUM_IDS);
	struct pub_sub_subscriber *subscriber = &f_subscriber->subscriber;
	struct msg_handler_data handler_data = {};
	struct k_poll_event poll_event;
	void *msg;
	int ret;

	// Add a subscriber and publish some messages to it
	pub_sub_subscriber_set_handler_data(subscriber, msg_handler, &handler_data);
	pub_sub_add_subscriber(subscriber);
	pub_sub_subscribe(subscriber, MSG_ID_SUBSCRIBED_ID_0);

	for (size_t i = 0; i < num_msgs; i++) {
		msg = pub_sub_new_msg(allocator, MSG_ID_SUBSCRIBED_ID_0, TEST_MSG_SIZE_BYTES,
				      K_NO_WAIT);
		zassert_not_null(msg);
		pub_sub_publish(msg);
	}

	// Should be able to poll for each message
	ret = pub_sub_populate_poll_evt(subscriber, &poll_event);
	zassert_ok(ret);
	handler_data.msg_id = MSG_ID_SUBSCRIBED_ID_0;
	for (size_t i = 0; i < num_msgs; i++) {
		poll_event.state = K_POLL_STATE_NOT_READY;
		ret = k_poll(&poll_event, 1, K_MSEC(1));
		zassert_ok(ret);
		ret = pub_sub_handle_queued_msg(subscriber, K_NO_WAIT);
		zassert_ok(ret);
	}

	// All the msgs have been handled so polling should fail
	poll_event.state = K_POLL_STATE_NOT_READY;
	ret = k_poll(&poll_event, 1, K_MSEC(1));
	zassert_not_ok(ret);
}

struct priority_data {
	struct pub_sub_subscriber *subscriber;
	uint8_t *shared_last_priority_value;
};

static void priority_handler(uint16_t msg_id, const void *msg, void *user_data)
{
	struct priority_data *priority_data = user_data;
	zassert_equal(*priority_data->shared_last_priority_value + 1,
		      priority_data->subscriber->priority);
	*priority_data->shared_last_priority_value = priority_data->subscriber->priority;
}

ZTEST(fifo, test_priority)
{
	struct pub_sub_allocator *allocator = &test_allocator;
	struct fifo_subscriber *f_subscribers[4] = {};
	struct priority_data priority_data[4] = {};
	uint8_t last_priority_value = 0;
	void *msg;

	// Create 4 subscribers with different priority values
	for (size_t i = 0; i < ARRAY_SIZE(f_subscribers); i++) {
		f_subscribers[i] = malloc_fifo_subscriber(MSG_ID_NUM_IDS);
		struct pub_sub_subscriber *subscriber = &f_subscribers[i]->subscriber;
		priority_data[i].subscriber = subscriber;
		priority_data[i].shared_last_priority_value = &last_priority_value;
		// Invert priority relative to order added
		pub_sub_subscriber_set_priority(subscriber, ARRAY_SIZE(f_subscribers) - i);
		pub_sub_subscriber_set_handler_data(subscriber, priority_handler,
						    &priority_data[i]);
		pub_sub_add_subscriber(subscriber);
		pub_sub_subscribe(subscriber, MSG_ID_SUBSCRIBED_ID_0);
	}

	// Publish a message
	msg = pub_sub_new_msg(allocator, MSG_ID_SUBSCRIBED_ID_0, TEST_MSG_SIZE_BYTES, K_NO_WAIT);
	zassert_not_null(msg);
	pub_sub_publish(msg);

	// Receive the message for each subscriber, need to iterate in priority order
	for (int i = ARRAY_SIZE(f_subscribers) - 1; i > -1; i--) {
		// Needs a small delay to allow the worker thread to run
		int ret = pub_sub_handle_queued_msg(&f_subscribers[i]->subscriber, K_MSEC(1));
		zassert_ok(ret);
	}
	zassert_equal(last_priority_value, 4);
}

ZTEST_SUITE(fifo, NULL, NULL, fifo_before_test, fifo_after_test, NULL);