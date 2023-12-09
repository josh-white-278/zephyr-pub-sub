/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <pub_sub/pub_sub.h>
#include <pub_sub/msg_alloc_mem_slab.h>
#include <zephyr/ztest.h>
#include <stdlib.h>
#include <helpers.h>

struct allocator_info {
	size_t msg_size;
	size_t num_msgs;
};

struct callbacks_fixture {
	struct allocator_info *alloc_infos;
	size_t num_allocators;
};

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

static void *callbacks_suite_setup(void)
{
	struct callbacks_fixture *test_fixture = malloc(sizeof(struct callbacks_fixture));
	// Create three mem_slab allocators of different msg sizes and number of msgs
	test_fixture->num_allocators = 3;
	test_fixture->alloc_infos =
		malloc(sizeof(*test_fixture->alloc_infos) * test_fixture->num_allocators);
	for (size_t i = 0; i < test_fixture->num_allocators; i++) {
		test_fixture->alloc_infos[i].msg_size = 8 << i;
		test_fixture->alloc_infos[i].num_msgs = 16 >> i;
	}
	return test_fixture;
}

static void callbacks_before_test(void *fixture)
{
	struct callbacks_fixture *test_fixture = fixture;
	reset_default_broker();
	// Create the mem slab allocators for the test
	for (size_t i = 0; i < test_fixture->num_allocators; i++) {
		struct pub_sub_allocator *allocator =
			malloc_mem_slab_allocator(test_fixture->alloc_infos[i].msg_size,
						  test_fixture->alloc_infos[i].num_msgs);
		pub_sub_add_allocator(allocator);
	}
}

static void callbacks_suite_teardown(void *fixture)
{
	struct callbacks_fixture *test_fixture = fixture;
	free(test_fixture->alloc_infos);
	free(test_fixture);
}

ZTEST_F(callbacks, test_add_remove_subscriber)
{
	struct callback_subscriber *c_subscriber = malloc_callback_subscriber(MSG_ID_NUM_IDS);
	struct pub_sub_subscriber *subscriber = &c_subscriber->subscriber;
	void *msg;
	struct rx_msg rx_msg;
	int ret;

	// Basic test that a subscriber receives a published message
	pub_sub_add_subscriber(subscriber);
	pub_sub_subscribe(subscriber, MSG_ID_SUBSCRIBED_ID_0);

	msg = pub_sub_new_msg(MSG_ID_SUBSCRIBED_ID_0, 8, K_NO_WAIT);
	zassert_not_null(msg);
	pub_sub_publish(msg);

	ret = k_msgq_get(&c_subscriber->msgq, &rx_msg,
			 K_MSEC(1)); // Needs a small delay to allow the worker thread to run
	zassert_ok(ret);
	zassert_equal(MSG_ID_SUBSCRIBED_ID_0, rx_msg.msg_id);
	zassert_equal_ptr(msg, rx_msg.msg);
	pub_sub_msg_dec_ref_cnt(rx_msg.msg);

	// Test that a removed subscriber stops receiving messages
	pub_sub_subscriber_remove_broker(subscriber);

	msg = pub_sub_new_msg(MSG_ID_SUBSCRIBED_ID_0, 8, K_NO_WAIT);
	zassert_not_null(msg);
	pub_sub_publish(msg);

	ret = k_msgq_get(&c_subscriber->msgq, &rx_msg,
			 K_MSEC(1)); // Needs a small delay to allow the worker thread to run
	zassert_not_ok(ret);

	// Test that a subscriber maintains its subscriptions and can just be
	// re-added to start receiving msgs again
	pub_sub_add_subscriber(subscriber);

	msg = pub_sub_new_msg(MSG_ID_SUBSCRIBED_ID_0, 8, K_NO_WAIT);
	zassert_not_null(msg);
	pub_sub_publish(msg);

	ret = k_msgq_get(&c_subscriber->msgq, &rx_msg,
			 K_MSEC(1)); // Needs a small delay to allow the worker thread to run
	zassert_ok(ret);
	zassert_equal(MSG_ID_SUBSCRIBED_ID_0, rx_msg.msg_id);
	zassert_equal_ptr(msg, rx_msg.msg);
	pub_sub_msg_dec_ref_cnt(rx_msg.msg);

	// Check we can't populate a poll event for a callback subscriber
	struct k_poll_event poll_event;
	ret = pub_sub_populate_poll_evt(subscriber, &poll_event);
	zassert_not_ok(ret);

	// Check we can't handle a queued message
	ret = pub_sub_handle_queued_msg(subscriber, K_MSEC(1));
	zassert_not_ok(ret);
}

ZTEST_F(callbacks, test_subscribing)
{
	struct callback_subscriber *c_subscriber = malloc_callback_subscriber(MSG_ID_NUM_IDS);
	struct pub_sub_subscriber *subscriber = &c_subscriber->subscriber;
	void *msg;
	struct rx_msg rx_msg;
	int ret;

	// Test that subscribed msg ids are received and others are not
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
		msg = pub_sub_new_msg(pub_ids[i], 8, K_NO_WAIT);
		zassert_not_null(msg);
		pub_sub_publish(msg);
	}

	// Should only receive MSG_ID_SUBSCRIBED_ID_* msgs
	for (size_t i = 0; i < ARRAY_SIZE(pub_ids); i += 2) {
		ret = k_msgq_get(
			&c_subscriber->msgq, &rx_msg,
			K_MSEC(1)); // Needs a small delay to allow the worker thread to run
		zassert_ok(ret);
		zassert_equal(pub_ids[i], rx_msg.msg_id);
		pub_sub_msg_dec_ref_cnt(rx_msg.msg);
	}

	// Unsubscribe from a couple of the IDs so the subscriber
	// should no longer receive those msg ids
	pub_sub_unsubscribe(subscriber, MSG_ID_SUBSCRIBED_ID_1);
	pub_sub_unsubscribe(subscriber, MSG_ID_SUBSCRIBED_ID_3);
	for (size_t i = 0; i < ARRAY_SIZE(pub_ids); i++) {
		msg = pub_sub_new_msg(pub_ids[i], 8, K_NO_WAIT);
		zassert_not_null(msg);
		pub_sub_publish(msg);
	}
	for (size_t i = 0; i < ARRAY_SIZE(pub_ids); i += 4) {
		ret = k_msgq_get(
			&c_subscriber->msgq, &rx_msg,
			K_MSEC(1)); // Needs a small delay to allow the worker thread to run
		zassert_ok(ret);
		zassert_equal(pub_ids[i], rx_msg.msg_id);
		pub_sub_msg_dec_ref_cnt(rx_msg.msg);
	}
	// No other messages in the queue
	ret = k_msgq_get(&c_subscriber->msgq, &rx_msg,
			 K_MSEC(1)); // Needs a small delay to allow the worker thread to run
	zassert_not_ok(ret);
}

ZTEST_F(callbacks, test_multi_subscriber)
{
	struct callback_subscriber *c_subscribers[4] = {};
	void *msg;
	struct rx_msg rx_msg;
	int ret;

	// Create 4 subscribers all subscribed to a unique msg id
	for (size_t i = 0; i < ARRAY_SIZE(c_subscribers); i++) {
		c_subscribers[i] = malloc_callback_subscriber(MSG_ID_NUM_IDS);
		struct pub_sub_subscriber *subscriber = &c_subscribers[i]->subscriber;
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
		msg = pub_sub_new_msg(pub_ids[i], 8, K_NO_WAIT);
		zassert_not_null(msg);
		pub_sub_publish(msg);
	}

	// Each subscriber should receive a single msg with their msg id
	for (size_t i = 0; i < ARRAY_SIZE(c_subscribers); i++) {
		struct k_msgq *msgq = &c_subscribers[i]->msgq;
		ret = k_msgq_get(
			msgq, &rx_msg,
			K_MSEC(1)); // Needs a small delay to allow the worker thread to run
		zassert_ok(ret);
		zassert_equal(pub_ids[i * 2], rx_msg.msg_id);
		pub_sub_msg_dec_ref_cnt(rx_msg.msg);
		// No other messages in the queue
		ret = k_msgq_get(
			msgq, &rx_msg,
			K_MSEC(1)); // Needs a small delay to allow the worker thread to run
		zassert_not_ok(ret);
	}

	// Have the subscribers subscribe to all of the messages
	for (size_t i = 0; i < ARRAY_SIZE(c_subscribers); i++) {
		struct pub_sub_subscriber *subscriber = &c_subscribers[i]->subscriber;
		pub_sub_subscribe(subscriber, MSG_ID_SUBSCRIBED_ID_0);
		pub_sub_subscribe(subscriber, MSG_ID_SUBSCRIBED_ID_1);
		pub_sub_subscribe(subscriber, MSG_ID_SUBSCRIBED_ID_2);
		pub_sub_subscribe(subscriber, MSG_ID_SUBSCRIBED_ID_3);
	}

	// Publish all of the messages
	for (size_t i = 0; i < ARRAY_SIZE(pub_ids); i++) {
		msg = pub_sub_new_msg(pub_ids[i], 8, K_NO_WAIT);
		zassert_not_null(msg);
		pub_sub_publish(msg);
	}

	// Each subscriber should receive each of the subscribed messages once
	for (size_t i = 0; i < ARRAY_SIZE(c_subscribers); i++) {
		struct k_msgq *msgq = &c_subscribers[i]->msgq;
		for (uint16_t msg_id = MSG_ID_SUBSCRIBED_ID_0; msg_id <= MSG_ID_SUBSCRIBED_ID_3;
		     msg_id += 2) {
			ret = k_msgq_get(
				msgq, &rx_msg,
				K_MSEC(1)); // Needs a small delay to allow the worker thread to run
			zassert_ok(ret);
			zassert_equal(msg_id, rx_msg.msg_id);
			pub_sub_msg_dec_ref_cnt(rx_msg.msg);
		}

		// No other messages in the queue
		ret = k_msgq_get(
			msgq, &rx_msg,
			K_MSEC(1)); // Needs a small delay to allow the worker thread to run
		zassert_not_ok(ret);
	}
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

ZTEST_F(callbacks, test_priority)
{
	struct callback_subscriber *c_subscribers[4] = {};
	struct priority_data priority_data[4] = {};
	uint8_t last_priority_value = 0;
	void *msg;

	// Create 4 subscribers with different priority values
	for (size_t i = 0; i < ARRAY_SIZE(c_subscribers); i++) {
		c_subscribers[i] = malloc_callback_subscriber(MSG_ID_NUM_IDS);
		struct pub_sub_subscriber *subscriber = &c_subscribers[i]->subscriber;
		priority_data[i].subscriber = subscriber;
		priority_data[i].shared_last_priority_value = &last_priority_value;
		// Invert priority relative to order added
		pub_sub_subscriber_set_priority(subscriber, ARRAY_SIZE(c_subscribers) - i);
		pub_sub_subscriber_set_handler_data(subscriber, priority_handler,
						    &priority_data[i]);
		pub_sub_add_subscriber(subscriber);
		pub_sub_subscribe(subscriber, MSG_ID_SUBSCRIBED_ID_0);
	}

	// Publish a message
	msg = pub_sub_new_msg(MSG_ID_SUBSCRIBED_ID_0, 8, K_NO_WAIT);
	zassert_not_null(msg);
	pub_sub_publish(msg);

	// Sleep to allow the publish to occur and the priority handler function to test the order
	// of subscribers receiving the message
	k_sleep(K_MSEC(1));
	zassert_equal(last_priority_value, 4);
}

ZTEST_SUITE(callbacks, NULL, callbacks_suite_setup, callbacks_before_test, NULL,
	    callbacks_suite_teardown);