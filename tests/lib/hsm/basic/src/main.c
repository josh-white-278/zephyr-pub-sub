/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <hsm/hsm.h>
#include <pub_sub/msg_alloc_mem_slab.h>
#include <zephyr/ztest.h>
#include <stdlib.h>

#define TEST_MSG_SIZE_BYTES 8

enum msg_id {
	MSG_ID_MAX_PUB_ID,
	MSG_ID_TEST_GET_CURRENT_STATE,
	MSG_ID_TEST_START_RX,
	MSG_ID_TEST_SUB_STATE_RX,
	MSG_ID_TEST_TOP_STATE_RX,
	MSG_ID_TEST_UNCONSUMED,
	MSG_ID_TEST_TRANSITION_START_STATE,
	MSG_ID_TEST_TRANSITION_SUB_STATE,
	MSG_ID_TEST_TRANSITION_TOP_STATE,
};

struct transition_msg {
	hsm_state_fn dest_state;
};

struct msg_rx_data {
	hsm_state_fn state_fn;
	uint16_t msg_id;
};

struct test_hsm {
	struct hsm hsm;
	struct msg_rx_data msg_rx_data[16];
	int num_msg_received;
};

PUB_SUB_MEM_SLAB_ALLOCATOR_DEFINE_STATIC(test_allocator, TEST_MSG_SIZE_BYTES, 32);
PUB_SUB_SUBS_BITARRAY_DEFINE(test_sub_bitarray, MSG_ID_MAX_PUB_ID);

static enum hsm_ret test_top_state(struct hsm *hsm, uint16_t msg_id, const void *msg)
{
	struct test_hsm *test_hsm = CONTAINER_OF(hsm, struct test_hsm, hsm);
	switch (msg_id) {
	case HSM_MSG_ID_ENTRY:
	case HSM_MSG_ID_EXIT:
	case MSG_ID_TEST_GET_CURRENT_STATE:
	case MSG_ID_TEST_TOP_STATE_RX:
		test_hsm->msg_rx_data[test_hsm->num_msg_received].state_fn = test_top_state;
		test_hsm->msg_rx_data[test_hsm->num_msg_received++].msg_id = msg_id;
		return HSM_CONSUMED();
	case MSG_ID_TEST_TRANSITION_TOP_STATE: {
		const struct transition_msg *transition_msg = msg;
		test_hsm->msg_rx_data[test_hsm->num_msg_received].state_fn = test_top_state;
		test_hsm->msg_rx_data[test_hsm->num_msg_received++].msg_id = msg_id;
		return HSM_TRANSITION(transition_msg->dest_state);
	}
	default:
		return HSM_TOP_STATE();
	}
}

static enum hsm_ret test_parent_0_far_state(struct hsm *hsm, uint16_t msg_id, const void *msg)
{
	struct test_hsm *test_hsm = CONTAINER_OF(hsm, struct test_hsm, hsm);
	switch (msg_id) {
	case HSM_MSG_ID_ENTRY:
	case HSM_MSG_ID_EXIT:
	case MSG_ID_TEST_GET_CURRENT_STATE:
		test_hsm->msg_rx_data[test_hsm->num_msg_received].state_fn =
			test_parent_0_far_state;
		test_hsm->msg_rx_data[test_hsm->num_msg_received++].msg_id = msg_id;
		return HSM_CONSUMED();
	default:
		return HSM_PARENT(test_top_state);
	}
}

static enum hsm_ret test_parent_1_far_state(struct hsm *hsm, uint16_t msg_id, const void *msg)
{
	struct test_hsm *test_hsm = CONTAINER_OF(hsm, struct test_hsm, hsm);
	switch (msg_id) {
	case HSM_MSG_ID_ENTRY:
	case HSM_MSG_ID_EXIT:
	case MSG_ID_TEST_GET_CURRENT_STATE:
		test_hsm->msg_rx_data[test_hsm->num_msg_received].state_fn =
			test_parent_1_far_state;
		test_hsm->msg_rx_data[test_hsm->num_msg_received++].msg_id = msg_id;
		return HSM_CONSUMED();
	default:
		return HSM_PARENT(test_parent_0_far_state);
	}
}

static enum hsm_ret test_far_state(struct hsm *hsm, uint16_t msg_id, const void *msg)
{
	struct test_hsm *test_hsm = CONTAINER_OF(hsm, struct test_hsm, hsm);
	switch (msg_id) {
	case HSM_MSG_ID_ENTRY:
	case HSM_MSG_ID_EXIT:
	case MSG_ID_TEST_GET_CURRENT_STATE:
		test_hsm->msg_rx_data[test_hsm->num_msg_received].state_fn = test_far_state;
		test_hsm->msg_rx_data[test_hsm->num_msg_received++].msg_id = msg_id;
		return HSM_CONSUMED();
	default:
		return HSM_PARENT(test_parent_1_far_state);
	}
}

static enum hsm_ret test_sub_state(struct hsm *hsm, uint16_t msg_id, const void *msg)
{
	struct test_hsm *test_hsm = CONTAINER_OF(hsm, struct test_hsm, hsm);
	switch (msg_id) {
	case HSM_MSG_ID_ENTRY:
	case HSM_MSG_ID_EXIT:
		test_hsm->msg_rx_data[test_hsm->num_msg_received].state_fn = test_sub_state;
		test_hsm->msg_rx_data[test_hsm->num_msg_received++].msg_id = msg_id;
		// Don't consume the entry message to test it doesn't propagate upwards
		// i.e. the top state doesn't get two at start
		return HSM_PARENT(test_top_state);
	case MSG_ID_TEST_GET_CURRENT_STATE:
	case MSG_ID_TEST_SUB_STATE_RX:
		test_hsm->msg_rx_data[test_hsm->num_msg_received].state_fn = test_sub_state;
		test_hsm->msg_rx_data[test_hsm->num_msg_received++].msg_id = msg_id;
		return HSM_CONSUMED();
	case MSG_ID_TEST_TRANSITION_SUB_STATE: {
		const struct transition_msg *transition_msg = msg;
		test_hsm->msg_rx_data[test_hsm->num_msg_received].state_fn = test_sub_state;
		test_hsm->msg_rx_data[test_hsm->num_msg_received++].msg_id = msg_id;
		return HSM_TRANSITION(transition_msg->dest_state);
	}
	default:
		return HSM_PARENT(test_top_state);
	}
}

static enum hsm_ret test_start_state(struct hsm *hsm, uint16_t msg_id, const void *msg)
{
	struct test_hsm *test_hsm = CONTAINER_OF(hsm, struct test_hsm, hsm);
	switch (msg_id) {
	case HSM_MSG_ID_ENTRY:
	case HSM_MSG_ID_EXIT:
	case MSG_ID_TEST_GET_CURRENT_STATE:
	case MSG_ID_TEST_START_RX:
		test_hsm->msg_rx_data[test_hsm->num_msg_received].state_fn = test_start_state;
		test_hsm->msg_rx_data[test_hsm->num_msg_received++].msg_id = msg_id;
		return HSM_CONSUMED();
	case MSG_ID_TEST_TRANSITION_START_STATE: {
		const struct transition_msg *transition_msg = msg;
		test_hsm->msg_rx_data[test_hsm->num_msg_received].state_fn = test_start_state;
		test_hsm->msg_rx_data[test_hsm->num_msg_received++].msg_id = msg_id;
		return HSM_TRANSITION(transition_msg->dest_state);
	}
	default:
		return HSM_PARENT(test_sub_state);
	}
}

static enum hsm_ret test_start_sibling_state(struct hsm *hsm, uint16_t msg_id, const void *msg)
{
	struct test_hsm *test_hsm = CONTAINER_OF(hsm, struct test_hsm, hsm);
	switch (msg_id) {
	case HSM_MSG_ID_ENTRY:
	case HSM_MSG_ID_EXIT:
	case MSG_ID_TEST_GET_CURRENT_STATE:
		test_hsm->msg_rx_data[test_hsm->num_msg_received].state_fn =
			test_start_sibling_state;
		test_hsm->msg_rx_data[test_hsm->num_msg_received++].msg_id = msg_id;
		return HSM_CONSUMED();
	default:
		return HSM_PARENT(test_sub_state);
	}
}

static enum hsm_ret test_start_child_state(struct hsm *hsm, uint16_t msg_id, const void *msg)
{
	struct test_hsm *test_hsm = CONTAINER_OF(hsm, struct test_hsm, hsm);
	switch (msg_id) {
	case HSM_MSG_ID_ENTRY:
	case HSM_MSG_ID_EXIT:
	case MSG_ID_TEST_GET_CURRENT_STATE:
		test_hsm->msg_rx_data[test_hsm->num_msg_received].state_fn = test_start_child_state;
		test_hsm->msg_rx_data[test_hsm->num_msg_received++].msg_id = msg_id;
		return HSM_CONSUMED();
	default:
		return HSM_PARENT(test_start_state);
	}
}

static enum hsm_ret test_start_child_of_child_state(struct hsm *hsm, uint16_t msg_id,
						    const void *msg)
{
	struct test_hsm *test_hsm = CONTAINER_OF(hsm, struct test_hsm, hsm);
	switch (msg_id) {
	case HSM_MSG_ID_ENTRY:
	case HSM_MSG_ID_EXIT:
	case MSG_ID_TEST_GET_CURRENT_STATE:
		test_hsm->msg_rx_data[test_hsm->num_msg_received].state_fn =
			test_start_child_of_child_state;
		test_hsm->msg_rx_data[test_hsm->num_msg_received++].msg_id = msg_id;
		return HSM_CONSUMED();
	default:
		return HSM_PARENT(test_start_child_state);
	}
}

static enum hsm_ret test_diff_top_state(struct hsm *hsm, uint16_t msg_id, const void *msg)
{
	struct test_hsm *test_hsm = CONTAINER_OF(hsm, struct test_hsm, hsm);
	switch (msg_id) {
	case HSM_MSG_ID_ENTRY:
	case HSM_MSG_ID_EXIT:
	case MSG_ID_TEST_GET_CURRENT_STATE:
		test_hsm->msg_rx_data[test_hsm->num_msg_received].state_fn = test_diff_top_state;
		test_hsm->msg_rx_data[test_hsm->num_msg_received++].msg_id = msg_id;
		return HSM_CONSUMED();
	default:
		return HSM_TOP_STATE();
	}
}

static enum hsm_ret test_diff_child_state(struct hsm *hsm, uint16_t msg_id, const void *msg)
{
	struct test_hsm *test_hsm = CONTAINER_OF(hsm, struct test_hsm, hsm);
	switch (msg_id) {
	case HSM_MSG_ID_ENTRY:
	case HSM_MSG_ID_EXIT:
	case MSG_ID_TEST_GET_CURRENT_STATE:
		test_hsm->msg_rx_data[test_hsm->num_msg_received].state_fn = test_diff_child_state;
		test_hsm->msg_rx_data[test_hsm->num_msg_received++].msg_id = msg_id;
		return HSM_CONSUMED();
	default:
		return HSM_PARENT(test_diff_top_state);
	}
}

static void publish_msg(struct hsm *hsm, uint16_t msg_id)
{
	void *msg = pub_sub_new_msg(&test_allocator, msg_id, TEST_MSG_SIZE_BYTES, K_NO_WAIT);
	zassert_not_null(msg);
	pub_sub_publish_to_subscriber(&hsm->subscriber, msg);
}

static void publish_transition_state(struct hsm *hsm, uint16_t msg_id, hsm_state_fn dest_state)
{
	struct transition_msg *msg =
		pub_sub_new_msg(&test_allocator, msg_id, sizeof(struct transition_msg), K_NO_WAIT);
	zassert_not_null(msg);
	msg->dest_state = dest_state;
	pub_sub_publish_to_subscriber(&hsm->subscriber, msg);
}

static void after_test(void *fixture)
{
	ARG_UNUSED(fixture);
	// Check for leaked messages
	struct k_mem_slab *mem_slab = test_allocator.impl;
	__ASSERT(k_mem_slab_num_used_get(mem_slab) == 0, "");
}

ZTEST(hsm_basic, test_start)
{
	struct test_hsm test_hsm = {};
	pub_sub_init_callback_subscriber(&test_hsm.hsm.subscriber, test_sub_bitarray,
					 MSG_ID_MAX_PUB_ID);
	hsm_init(&test_hsm.hsm, test_start_state);
	hsm_start(&test_hsm.hsm);
	// Expect all parents and start state to have received entry message
	zassert_equal(test_hsm.num_msg_received, 3);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_top_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, HSM_MSG_ID_ENTRY);
	zassert_equal(test_hsm.msg_rx_data[1].state_fn, test_sub_state);
	zassert_equal(test_hsm.msg_rx_data[1].msg_id, HSM_MSG_ID_ENTRY);
	zassert_equal(test_hsm.msg_rx_data[2].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[2].msg_id, HSM_MSG_ID_ENTRY);
}

ZTEST(hsm_basic, test_current_state_rx)
{
	struct test_hsm test_hsm = {};
	pub_sub_init_callback_subscriber(&test_hsm.hsm.subscriber, test_sub_bitarray,
					 MSG_ID_MAX_PUB_ID);
	hsm_init(&test_hsm.hsm, test_start_state);
	hsm_start(&test_hsm.hsm);
	test_hsm.num_msg_received = 0;

	publish_msg(&test_hsm.hsm, MSG_ID_TEST_START_RX);
	zassert_equal(test_hsm.num_msg_received, 1);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_START_RX);
}

ZTEST(hsm_basic, test_parent_state_rx)
{
	struct test_hsm test_hsm = {};
	pub_sub_init_callback_subscriber(&test_hsm.hsm.subscriber, test_sub_bitarray,
					 MSG_ID_MAX_PUB_ID);
	hsm_init(&test_hsm.hsm, test_start_state);
	hsm_start(&test_hsm.hsm);
	test_hsm.num_msg_received = 0;

	publish_msg(&test_hsm.hsm, MSG_ID_TEST_SUB_STATE_RX);
	zassert_equal(test_hsm.num_msg_received, 1);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_sub_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_SUB_STATE_RX);
}

ZTEST(hsm_basic, test_top_state_rx)
{
	struct test_hsm test_hsm = {};
	pub_sub_init_callback_subscriber(&test_hsm.hsm.subscriber, test_sub_bitarray,
					 MSG_ID_MAX_PUB_ID);
	hsm_init(&test_hsm.hsm, test_start_state);
	hsm_start(&test_hsm.hsm);
	test_hsm.num_msg_received = 0;

	publish_msg(&test_hsm.hsm, MSG_ID_TEST_TOP_STATE_RX);
	zassert_equal(test_hsm.num_msg_received, 1);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_top_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_TOP_STATE_RX);
}

ZTEST(hsm_basic, test_unconsumed)
{
	struct test_hsm test_hsm = {};
	pub_sub_init_callback_subscriber(&test_hsm.hsm.subscriber, test_sub_bitarray,
					 MSG_ID_MAX_PUB_ID);
	hsm_init(&test_hsm.hsm, test_start_state);
	hsm_start(&test_hsm.hsm);
	test_hsm.num_msg_received = 0;

	publish_msg(&test_hsm.hsm, MSG_ID_TEST_UNCONSUMED);
	zassert_equal(test_hsm.num_msg_received, 0);
}

ZTEST(hsm_basic, test_transition_to_current)
{
	struct test_hsm test_hsm = {};
	pub_sub_init_callback_subscriber(&test_hsm.hsm.subscriber, test_sub_bitarray,
					 MSG_ID_MAX_PUB_ID);
	hsm_init(&test_hsm.hsm, test_start_state);
	hsm_start(&test_hsm.hsm);
	test_hsm.num_msg_received = 0;

	publish_transition_state(&test_hsm.hsm, MSG_ID_TEST_TRANSITION_START_STATE,
				 test_start_state);
	zassert_equal(test_hsm.num_msg_received, 1);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_TRANSITION_START_STATE);

	test_hsm.num_msg_received = 0;
	publish_msg(&test_hsm.hsm, MSG_ID_TEST_GET_CURRENT_STATE);
	zassert_equal(test_hsm.num_msg_received, 1);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_GET_CURRENT_STATE);
}

ZTEST(hsm_basic, test_transition_to_child)
{
	struct test_hsm test_hsm = {};
	pub_sub_init_callback_subscriber(&test_hsm.hsm.subscriber, test_sub_bitarray,
					 MSG_ID_MAX_PUB_ID);
	hsm_init(&test_hsm.hsm, test_start_state);
	hsm_start(&test_hsm.hsm);
	test_hsm.num_msg_received = 0;

	publish_transition_state(&test_hsm.hsm, MSG_ID_TEST_TRANSITION_START_STATE,
				 test_start_child_state);
	// We expect 2 msgs, MSG_ID_TEST_TRANSITION_START_STATE then ENTRY to child
	zassert_equal(test_hsm.num_msg_received, 2);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_TRANSITION_START_STATE);
	zassert_equal(test_hsm.msg_rx_data[1].state_fn, test_start_child_state);
	zassert_equal(test_hsm.msg_rx_data[1].msg_id, HSM_MSG_ID_ENTRY);

	test_hsm.num_msg_received = 0;
	publish_msg(&test_hsm.hsm, MSG_ID_TEST_GET_CURRENT_STATE);
	zassert_equal(test_hsm.num_msg_received, 1);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_start_child_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_GET_CURRENT_STATE);
}

ZTEST(hsm_basic, test_transition_to_child_of_child)
{
	struct test_hsm test_hsm = {};
	pub_sub_init_callback_subscriber(&test_hsm.hsm.subscriber, test_sub_bitarray,
					 MSG_ID_MAX_PUB_ID);
	hsm_init(&test_hsm.hsm, test_start_state);
	hsm_start(&test_hsm.hsm);
	test_hsm.num_msg_received = 0;

	publish_transition_state(&test_hsm.hsm, MSG_ID_TEST_TRANSITION_START_STATE,
				 test_start_child_of_child_state);
	// We expect 3 msgs, MSG_ID_TEST_TRANSITION_START_STATE then ENTRY to child and child of
	// child
	zassert_equal(test_hsm.num_msg_received, 3);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_TRANSITION_START_STATE);
	zassert_equal(test_hsm.msg_rx_data[1].state_fn, test_start_child_state);
	zassert_equal(test_hsm.msg_rx_data[1].msg_id, HSM_MSG_ID_ENTRY);
	zassert_equal(test_hsm.msg_rx_data[2].state_fn, test_start_child_of_child_state);
	zassert_equal(test_hsm.msg_rx_data[2].msg_id, HSM_MSG_ID_ENTRY);

	test_hsm.num_msg_received = 0;
	publish_msg(&test_hsm.hsm, MSG_ID_TEST_GET_CURRENT_STATE);
	zassert_equal(test_hsm.num_msg_received, 1);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_start_child_of_child_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_GET_CURRENT_STATE);
}

ZTEST(hsm_basic, test_transition_to_sibling)
{
	struct test_hsm test_hsm = {};
	pub_sub_init_callback_subscriber(&test_hsm.hsm.subscriber, test_sub_bitarray,
					 MSG_ID_MAX_PUB_ID);
	hsm_init(&test_hsm.hsm, test_start_state);
	hsm_start(&test_hsm.hsm);
	test_hsm.num_msg_received = 0;

	publish_transition_state(&test_hsm.hsm, MSG_ID_TEST_TRANSITION_START_STATE,
				 test_start_sibling_state);
	// We expect 3 msgs, MSG_ID_TEST_TRANSITION_START_STATE then EXIT from start and ENTRY to
	// sibling
	zassert_equal(test_hsm.num_msg_received, 3);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_TRANSITION_START_STATE);
	zassert_equal(test_hsm.msg_rx_data[1].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[1].msg_id, HSM_MSG_ID_EXIT);
	zassert_equal(test_hsm.msg_rx_data[2].state_fn, test_start_sibling_state);
	zassert_equal(test_hsm.msg_rx_data[2].msg_id, HSM_MSG_ID_ENTRY);

	test_hsm.num_msg_received = 0;
	publish_msg(&test_hsm.hsm, MSG_ID_TEST_GET_CURRENT_STATE);
	zassert_equal(test_hsm.num_msg_received, 1);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_start_sibling_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_GET_CURRENT_STATE);
}

ZTEST(hsm_basic, test_transition_to_far_state)
{
	struct test_hsm test_hsm = {};
	pub_sub_init_callback_subscriber(&test_hsm.hsm.subscriber, test_sub_bitarray,
					 MSG_ID_MAX_PUB_ID);
	hsm_init(&test_hsm.hsm, test_start_state);
	hsm_start(&test_hsm.hsm);
	test_hsm.num_msg_received = 0;

	publish_transition_state(&test_hsm.hsm, MSG_ID_TEST_TRANSITION_START_STATE, test_far_state);
	// We expect 6 msgs:
	// start_state: MSG_ID_TEST_TRANSITION_START_STATE
	// start_state, sub_state: EXIT
	// parent_0_far_state, parent_0_far_state, far_state: ENTRY
	zassert_equal(test_hsm.num_msg_received, 6);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_TRANSITION_START_STATE);
	zassert_equal(test_hsm.msg_rx_data[1].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[1].msg_id, HSM_MSG_ID_EXIT);
	zassert_equal(test_hsm.msg_rx_data[2].state_fn, test_sub_state);
	zassert_equal(test_hsm.msg_rx_data[2].msg_id, HSM_MSG_ID_EXIT);
	zassert_equal(test_hsm.msg_rx_data[3].state_fn, test_parent_0_far_state);
	zassert_equal(test_hsm.msg_rx_data[3].msg_id, HSM_MSG_ID_ENTRY);
	zassert_equal(test_hsm.msg_rx_data[4].state_fn, test_parent_1_far_state);
	zassert_equal(test_hsm.msg_rx_data[4].msg_id, HSM_MSG_ID_ENTRY);
	zassert_equal(test_hsm.msg_rx_data[5].state_fn, test_far_state);
	zassert_equal(test_hsm.msg_rx_data[5].msg_id, HSM_MSG_ID_ENTRY);

	test_hsm.num_msg_received = 0;
	publish_msg(&test_hsm.hsm, MSG_ID_TEST_GET_CURRENT_STATE);
	zassert_equal(test_hsm.num_msg_received, 1);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_far_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_GET_CURRENT_STATE);
}

ZTEST(hsm_basic, test_transition_to_different_hsm)
{
	struct test_hsm test_hsm = {};
	pub_sub_init_callback_subscriber(&test_hsm.hsm.subscriber, test_sub_bitarray,
					 MSG_ID_MAX_PUB_ID);
	hsm_init(&test_hsm.hsm, test_start_state);
	hsm_start(&test_hsm.hsm);
	test_hsm.num_msg_received = 0;

	publish_transition_state(&test_hsm.hsm, MSG_ID_TEST_TRANSITION_START_STATE,
				 test_diff_child_state);
	// We expect 6 msgs:
	// start_state: MSG_ID_TEST_TRANSITION_START_STATE
	// start_state, sub_state, top_state: EXIT
	// diff_top_state, diff_child_state: ENTRY
	zassert_equal(test_hsm.num_msg_received, 6);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_TRANSITION_START_STATE);
	zassert_equal(test_hsm.msg_rx_data[1].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[1].msg_id, HSM_MSG_ID_EXIT);
	zassert_equal(test_hsm.msg_rx_data[2].state_fn, test_sub_state);
	zassert_equal(test_hsm.msg_rx_data[2].msg_id, HSM_MSG_ID_EXIT);
	zassert_equal(test_hsm.msg_rx_data[3].state_fn, test_top_state);
	zassert_equal(test_hsm.msg_rx_data[3].msg_id, HSM_MSG_ID_EXIT);
	zassert_equal(test_hsm.msg_rx_data[4].state_fn, test_diff_top_state);
	zassert_equal(test_hsm.msg_rx_data[4].msg_id, HSM_MSG_ID_ENTRY);
	zassert_equal(test_hsm.msg_rx_data[5].state_fn, test_diff_child_state);
	zassert_equal(test_hsm.msg_rx_data[5].msg_id, HSM_MSG_ID_ENTRY);

	test_hsm.num_msg_received = 0;
	publish_msg(&test_hsm.hsm, MSG_ID_TEST_GET_CURRENT_STATE);
	zassert_equal(test_hsm.num_msg_received, 1);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_diff_child_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_GET_CURRENT_STATE);
}

ZTEST(hsm_basic, test_transition_from_parent)
{
	struct test_hsm test_hsm = {};
	pub_sub_init_callback_subscriber(&test_hsm.hsm.subscriber, test_sub_bitarray,
					 MSG_ID_MAX_PUB_ID);
	hsm_init(&test_hsm.hsm, test_start_state);
	hsm_start(&test_hsm.hsm);
	test_hsm.num_msg_received = 0;

	publish_transition_state(&test_hsm.hsm, MSG_ID_TEST_TRANSITION_SUB_STATE, test_far_state);
	// We expect 6 msgs:
	// sub_state: MSG_ID_TEST_TRANSITION_SUB_STATE
	// start_state, sub_state: EXIT
	// parent_0_far_state, parent_0_far_state, far_state: ENTRY
	zassert_equal(test_hsm.num_msg_received, 6);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_sub_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_TRANSITION_SUB_STATE);
	zassert_equal(test_hsm.msg_rx_data[1].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[1].msg_id, HSM_MSG_ID_EXIT);
	zassert_equal(test_hsm.msg_rx_data[2].state_fn, test_sub_state);
	zassert_equal(test_hsm.msg_rx_data[2].msg_id, HSM_MSG_ID_EXIT);
	zassert_equal(test_hsm.msg_rx_data[3].state_fn, test_parent_0_far_state);
	zassert_equal(test_hsm.msg_rx_data[3].msg_id, HSM_MSG_ID_ENTRY);
	zassert_equal(test_hsm.msg_rx_data[4].state_fn, test_parent_1_far_state);
	zassert_equal(test_hsm.msg_rx_data[4].msg_id, HSM_MSG_ID_ENTRY);
	zassert_equal(test_hsm.msg_rx_data[5].state_fn, test_far_state);
	zassert_equal(test_hsm.msg_rx_data[5].msg_id, HSM_MSG_ID_ENTRY);

	test_hsm.num_msg_received = 0;
	publish_msg(&test_hsm.hsm, MSG_ID_TEST_GET_CURRENT_STATE);
	zassert_equal(test_hsm.num_msg_received, 1);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_far_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_GET_CURRENT_STATE);
}

ZTEST(hsm_basic, test_transition_from_top)
{
	struct test_hsm test_hsm = {};
	pub_sub_init_callback_subscriber(&test_hsm.hsm.subscriber, test_sub_bitarray,
					 MSG_ID_MAX_PUB_ID);
	hsm_init(&test_hsm.hsm, test_start_state);
	hsm_start(&test_hsm.hsm);
	test_hsm.num_msg_received = 0;

	publish_transition_state(&test_hsm.hsm, MSG_ID_TEST_TRANSITION_TOP_STATE, test_far_state);
	// We expect 6 msgs:
	// top_state: MSG_ID_TEST_TRANSITION_TOP_STATE
	// start_state, sub_state: EXIT
	// parent_0_far_state, parent_0_far_state, far_state: ENTRY
	zassert_equal(test_hsm.num_msg_received, 6);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_top_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_TRANSITION_TOP_STATE);
	zassert_equal(test_hsm.msg_rx_data[1].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[1].msg_id, HSM_MSG_ID_EXIT);
	zassert_equal(test_hsm.msg_rx_data[2].state_fn, test_sub_state);
	zassert_equal(test_hsm.msg_rx_data[2].msg_id, HSM_MSG_ID_EXIT);
	zassert_equal(test_hsm.msg_rx_data[3].state_fn, test_parent_0_far_state);
	zassert_equal(test_hsm.msg_rx_data[3].msg_id, HSM_MSG_ID_ENTRY);
	zassert_equal(test_hsm.msg_rx_data[4].state_fn, test_parent_1_far_state);
	zassert_equal(test_hsm.msg_rx_data[4].msg_id, HSM_MSG_ID_ENTRY);
	zassert_equal(test_hsm.msg_rx_data[5].state_fn, test_far_state);
	zassert_equal(test_hsm.msg_rx_data[5].msg_id, HSM_MSG_ID_ENTRY);

	test_hsm.num_msg_received = 0;
	publish_msg(&test_hsm.hsm, MSG_ID_TEST_GET_CURRENT_STATE);
	zassert_equal(test_hsm.num_msg_received, 1);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_far_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_GET_CURRENT_STATE);
}

static enum hsm_ret test_recursive_state(struct hsm *hsm, uint16_t msg_id, const void *msg)
{
	struct test_hsm *test_hsm = CONTAINER_OF(hsm, struct test_hsm, hsm);
	switch (msg_id) {
	case HSM_MSG_ID_ENTRY:
	case HSM_MSG_ID_EXIT:
	case MSG_ID_TEST_GET_CURRENT_STATE:
		test_hsm->msg_rx_data[test_hsm->num_msg_received].state_fn = test_recursive_state;
		test_hsm->msg_rx_data[test_hsm->num_msg_received++].msg_id = msg_id;
		return HSM_CONSUMED();
	case MSG_ID_TEST_TRANSITION_START_STATE: {
		const struct transition_msg *transition_msg = msg;
		test_hsm->msg_rx_data[test_hsm->num_msg_received].state_fn = test_recursive_state;
		test_hsm->msg_rx_data[test_hsm->num_msg_received++].msg_id = msg_id;
		return HSM_TRANSITION(transition_msg->dest_state);
	}
	default:
		// Make this state its own parent to test maximum state depth
		return HSM_PARENT(test_recursive_state);
	}
}

ZTEST(hsm_basic, test_state_depth)
{
	struct test_hsm test_hsm = {};
	pub_sub_init_callback_subscriber(&test_hsm.hsm.subscriber, test_sub_bitarray,
					 MSG_ID_MAX_PUB_ID);
	hsm_init(&test_hsm.hsm, test_recursive_state);
	hsm_start(&test_hsm.hsm);
	// Expect CONFIG_HSM_MAX_NESTED_STATES start messages
	zassert_equal(test_hsm.num_msg_received, CONFIG_HSM_MAX_NESTED_STATES);
	for (size_t i = 0; i < CONFIG_HSM_MAX_NESTED_STATES; i++) {
		zassert_equal(test_hsm.msg_rx_data[i].state_fn, test_recursive_state);
		zassert_equal(test_hsm.msg_rx_data[i].msg_id, HSM_MSG_ID_ENTRY);
	}

	test_hsm.num_msg_received = 0;
	publish_transition_state(&test_hsm.hsm, MSG_ID_TEST_TRANSITION_START_STATE,
				 test_start_state);
	// We expect the following messages:
	// test_recursive_state: MSG_ID_TEST_TRANSITION_TOP_STATE
	// CONFIG_HSM_MAX_NESTED_STATES of test_recursive_state: EXIT
	// top_state, sub_state, start_state: ENTRY
	zassert_equal(test_hsm.num_msg_received, 1 + CONFIG_HSM_MAX_NESTED_STATES + 3);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_recursive_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_TRANSITION_START_STATE);
	for (size_t i = 0; i < CONFIG_HSM_MAX_NESTED_STATES; i++) {
		zassert_equal(test_hsm.msg_rx_data[i + 1].state_fn, test_recursive_state);
		zassert_equal(test_hsm.msg_rx_data[i + 1].msg_id, HSM_MSG_ID_EXIT);
	}
	zassert_equal(test_hsm.msg_rx_data[CONFIG_HSM_MAX_NESTED_STATES + 1].state_fn,
		      test_top_state);
	zassert_equal(test_hsm.msg_rx_data[CONFIG_HSM_MAX_NESTED_STATES + 1].msg_id,
		      HSM_MSG_ID_ENTRY);
	zassert_equal(test_hsm.msg_rx_data[CONFIG_HSM_MAX_NESTED_STATES + 2].state_fn,
		      test_sub_state);
	zassert_equal(test_hsm.msg_rx_data[CONFIG_HSM_MAX_NESTED_STATES + 2].msg_id,
		      HSM_MSG_ID_ENTRY);
	zassert_equal(test_hsm.msg_rx_data[CONFIG_HSM_MAX_NESTED_STATES + 3].state_fn,
		      test_start_state);
	zassert_equal(test_hsm.msg_rx_data[CONFIG_HSM_MAX_NESTED_STATES + 3].msg_id,
		      HSM_MSG_ID_ENTRY);

	test_hsm.num_msg_received = 0;
	publish_msg(&test_hsm.hsm, MSG_ID_TEST_GET_CURRENT_STATE);
	zassert_equal(test_hsm.num_msg_received, 1);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_GET_CURRENT_STATE);
}

ZTEST_SUITE(hsm_basic, NULL, NULL, NULL, after_test, NULL);