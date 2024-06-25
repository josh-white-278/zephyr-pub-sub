/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <hsm/hsm.h>
#include <zephyr/ztest.h>
#include <stdlib.h>

enum msg_id {
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

struct test_hsm test_hsm;

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

static enum hsm_ret test_trans_on_entry_to_start(struct hsm *hsm, uint16_t msg_id, const void *msg)
{
	struct test_hsm *test_hsm = CONTAINER_OF(hsm, struct test_hsm, hsm);
	switch (msg_id) {
	case HSM_MSG_ID_ENTRY:
		test_hsm->msg_rx_data[test_hsm->num_msg_received].state_fn =
			test_trans_on_entry_to_start;
		test_hsm->msg_rx_data[test_hsm->num_msg_received++].msg_id = msg_id;
		return HSM_TRANSITION(test_start_state);
	case HSM_MSG_ID_EXIT:
	case MSG_ID_TEST_GET_CURRENT_STATE:
		test_hsm->msg_rx_data[test_hsm->num_msg_received].state_fn =
			test_trans_on_entry_to_start;
		test_hsm->msg_rx_data[test_hsm->num_msg_received++].msg_id = msg_id;
		return HSM_CONSUMED();
	default:
		return HSM_PARENT(test_top_state);
	}
}

static enum hsm_ret test_trans_on_entry_child_state(struct hsm *hsm, uint16_t msg_id,
						    const void *msg)
{
	struct test_hsm *test_hsm = CONTAINER_OF(hsm, struct test_hsm, hsm);
	switch (msg_id) {
	case HSM_MSG_ID_ENTRY:
	case HSM_MSG_ID_EXIT:
	case MSG_ID_TEST_GET_CURRENT_STATE:
		test_hsm->msg_rx_data[test_hsm->num_msg_received].state_fn =
			test_trans_on_entry_child_state;
		test_hsm->msg_rx_data[test_hsm->num_msg_received++].msg_id = msg_id;
		return HSM_CONSUMED();
	default:
		return HSM_PARENT(test_trans_on_entry_to_start);
	}
}

static enum hsm_ret test_trans_on_entry_to_trans_on_entry_child(struct hsm *hsm, uint16_t msg_id,
								const void *msg)
{
	struct test_hsm *test_hsm = CONTAINER_OF(hsm, struct test_hsm, hsm);
	switch (msg_id) {
	case HSM_MSG_ID_ENTRY:
		test_hsm->msg_rx_data[test_hsm->num_msg_received].state_fn =
			test_trans_on_entry_to_trans_on_entry_child;
		test_hsm->msg_rx_data[test_hsm->num_msg_received++].msg_id = msg_id;
		return HSM_TRANSITION(test_trans_on_entry_child_state);
	case HSM_MSG_ID_EXIT:
	case MSG_ID_TEST_GET_CURRENT_STATE:
		test_hsm->msg_rx_data[test_hsm->num_msg_received].state_fn =
			test_trans_on_entry_to_trans_on_entry_child;
		test_hsm->msg_rx_data[test_hsm->num_msg_received++].msg_id = msg_id;
		return HSM_CONSUMED();
	default:
		return HSM_PARENT(test_top_state);
	}
}

static void publish_msg(struct hsm *hsm, uint16_t msg_id)
{
	hsm_run(hsm, msg_id, NULL);
}

static void publish_transition_state(struct hsm *hsm, uint16_t msg_id, hsm_state_fn dest_state)
{
	struct transition_msg msg = {
		.dest_state = dest_state,
	};
	hsm_run(hsm, msg_id, &msg);
}

static void before_test(void *fixture)
{
	ARG_UNUSED(fixture);
	hsm_start(&test_hsm.hsm, test_start_state);
	test_hsm.num_msg_received = 0;
}

ZTEST(hsm_basic, test_start)
{
	hsm_start(&test_hsm.hsm, test_start_state);
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
	publish_msg(&test_hsm.hsm, MSG_ID_TEST_START_RX);
	zassert_equal(test_hsm.num_msg_received, 1);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_START_RX);
}

ZTEST(hsm_basic, test_parent_state_rx)
{
	publish_msg(&test_hsm.hsm, MSG_ID_TEST_SUB_STATE_RX);
	zassert_equal(test_hsm.num_msg_received, 1);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_sub_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_SUB_STATE_RX);
}

ZTEST(hsm_basic, test_top_state_rx)
{
	publish_msg(&test_hsm.hsm, MSG_ID_TEST_TOP_STATE_RX);
	zassert_equal(test_hsm.num_msg_received, 1);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_top_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_TOP_STATE_RX);
}

ZTEST(hsm_basic, test_unconsumed)
{
	publish_msg(&test_hsm.hsm, MSG_ID_TEST_UNCONSUMED);
	zassert_equal(test_hsm.num_msg_received, 0);
}

ZTEST(hsm_basic, test_transition_to_current)
{
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

ZTEST(hsm_basic, test_transition_on_entry_simple)
{
	publish_transition_state(&test_hsm.hsm, MSG_ID_TEST_TRANSITION_START_STATE,
				 test_trans_on_entry_to_start);
	// We expect 7 msgs:
	// start_state: MSG_ID_TEST_TRANSITION_START_STATE
	// start_state, sub_state: EXIT
	// trans_on_entry_to_start: ENTRY,EXIT
	// sub_state, start_state: ENTRY
	zassert_equal(test_hsm.num_msg_received, 7);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_TRANSITION_START_STATE);
	zassert_equal(test_hsm.msg_rx_data[1].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[1].msg_id, HSM_MSG_ID_EXIT);
	zassert_equal(test_hsm.msg_rx_data[2].state_fn, test_sub_state);
	zassert_equal(test_hsm.msg_rx_data[2].msg_id, HSM_MSG_ID_EXIT);
	zassert_equal(test_hsm.msg_rx_data[3].state_fn, test_trans_on_entry_to_start);
	zassert_equal(test_hsm.msg_rx_data[3].msg_id, HSM_MSG_ID_ENTRY);
	zassert_equal(test_hsm.msg_rx_data[4].state_fn, test_trans_on_entry_to_start);
	zassert_equal(test_hsm.msg_rx_data[4].msg_id, HSM_MSG_ID_EXIT);
	zassert_equal(test_hsm.msg_rx_data[5].state_fn, test_sub_state);
	zassert_equal(test_hsm.msg_rx_data[5].msg_id, HSM_MSG_ID_ENTRY);
	zassert_equal(test_hsm.msg_rx_data[6].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[6].msg_id, HSM_MSG_ID_ENTRY);

	test_hsm.num_msg_received = 0;
	publish_msg(&test_hsm.hsm, MSG_ID_TEST_GET_CURRENT_STATE);
	zassert_equal(test_hsm.num_msg_received, 1);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_GET_CURRENT_STATE);
}

ZTEST(hsm_basic, test_transition_on_entry_parent)
{
	publish_transition_state(&test_hsm.hsm, MSG_ID_TEST_TRANSITION_START_STATE,
				 test_trans_on_entry_child_state);
	// We expect 7 msgs:
	// start_state: MSG_ID_TEST_TRANSITION_START_STATE
	// start_state, sub_state: EXIT
	// trans_on_entry_to_start: ENTRY,EXIT
	// sub_state, start_state: ENTRY
	zassert_equal(test_hsm.num_msg_received, 7);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_TRANSITION_START_STATE);
	zassert_equal(test_hsm.msg_rx_data[1].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[1].msg_id, HSM_MSG_ID_EXIT);
	zassert_equal(test_hsm.msg_rx_data[2].state_fn, test_sub_state);
	zassert_equal(test_hsm.msg_rx_data[2].msg_id, HSM_MSG_ID_EXIT);
	zassert_equal(test_hsm.msg_rx_data[3].state_fn, test_trans_on_entry_to_start);
	zassert_equal(test_hsm.msg_rx_data[3].msg_id, HSM_MSG_ID_ENTRY);
	zassert_equal(test_hsm.msg_rx_data[4].state_fn, test_trans_on_entry_to_start);
	zassert_equal(test_hsm.msg_rx_data[4].msg_id, HSM_MSG_ID_EXIT);
	zassert_equal(test_hsm.msg_rx_data[5].state_fn, test_sub_state);
	zassert_equal(test_hsm.msg_rx_data[5].msg_id, HSM_MSG_ID_ENTRY);
	zassert_equal(test_hsm.msg_rx_data[6].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[6].msg_id, HSM_MSG_ID_ENTRY);

	test_hsm.num_msg_received = 0;
	publish_msg(&test_hsm.hsm, MSG_ID_TEST_GET_CURRENT_STATE);
	zassert_equal(test_hsm.num_msg_received, 1);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_GET_CURRENT_STATE);
}

ZTEST(hsm_basic, test_transition_on_entry_double)
{
	publish_transition_state(&test_hsm.hsm, MSG_ID_TEST_TRANSITION_START_STATE,
				 test_trans_on_entry_to_trans_on_entry_child);
	// We expect 9 msgs:
	// start_state: MSG_ID_TEST_TRANSITION_START_STATE
	// start_state, sub_state: EXIT
	// trans_on_entry_to_trans_on_entry_child: ENTRY,EXIT
	// trans_on_entry_to_start: ENTRY,EXIT
	// sub_state, start_state: ENTRY
	zassert_equal(test_hsm.num_msg_received, 9);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_TRANSITION_START_STATE);
	zassert_equal(test_hsm.msg_rx_data[1].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[1].msg_id, HSM_MSG_ID_EXIT);
	zassert_equal(test_hsm.msg_rx_data[2].state_fn, test_sub_state);
	zassert_equal(test_hsm.msg_rx_data[2].msg_id, HSM_MSG_ID_EXIT);
	zassert_equal(test_hsm.msg_rx_data[3].state_fn,
		      test_trans_on_entry_to_trans_on_entry_child);
	zassert_equal(test_hsm.msg_rx_data[3].msg_id, HSM_MSG_ID_ENTRY);
	zassert_equal(test_hsm.msg_rx_data[4].state_fn,
		      test_trans_on_entry_to_trans_on_entry_child);
	zassert_equal(test_hsm.msg_rx_data[4].msg_id, HSM_MSG_ID_EXIT);
	zassert_equal(test_hsm.msg_rx_data[5].state_fn, test_trans_on_entry_to_start);
	zassert_equal(test_hsm.msg_rx_data[5].msg_id, HSM_MSG_ID_ENTRY);
	zassert_equal(test_hsm.msg_rx_data[6].state_fn, test_trans_on_entry_to_start);
	zassert_equal(test_hsm.msg_rx_data[6].msg_id, HSM_MSG_ID_EXIT);
	zassert_equal(test_hsm.msg_rx_data[7].state_fn, test_sub_state);
	zassert_equal(test_hsm.msg_rx_data[7].msg_id, HSM_MSG_ID_ENTRY);
	zassert_equal(test_hsm.msg_rx_data[8].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[8].msg_id, HSM_MSG_ID_ENTRY);

	test_hsm.num_msg_received = 0;
	publish_msg(&test_hsm.hsm, MSG_ID_TEST_GET_CURRENT_STATE);
	zassert_equal(test_hsm.num_msg_received, 1);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_GET_CURRENT_STATE);
}

ZTEST(hsm_basic, test_transition_on_entry_start)
{
	hsm_start(&test_hsm.hsm, test_trans_on_entry_to_start);

	// We expect 5 msgs:
	// top_state: ENTRY
	// trans_on_entry_to_start: ENTRY,EXIT
	// sub_state, start_state: ENTRY
	zassert_equal(test_hsm.num_msg_received, 5);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_top_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, HSM_MSG_ID_ENTRY);
	zassert_equal(test_hsm.msg_rx_data[1].state_fn, test_trans_on_entry_to_start);
	zassert_equal(test_hsm.msg_rx_data[1].msg_id, HSM_MSG_ID_ENTRY);
	zassert_equal(test_hsm.msg_rx_data[2].state_fn, test_trans_on_entry_to_start);
	zassert_equal(test_hsm.msg_rx_data[2].msg_id, HSM_MSG_ID_EXIT);
	zassert_equal(test_hsm.msg_rx_data[3].state_fn, test_sub_state);
	zassert_equal(test_hsm.msg_rx_data[3].msg_id, HSM_MSG_ID_ENTRY);
	zassert_equal(test_hsm.msg_rx_data[4].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[4].msg_id, HSM_MSG_ID_ENTRY);

	test_hsm.num_msg_received = 0;
	publish_msg(&test_hsm.hsm, MSG_ID_TEST_GET_CURRENT_STATE);
	zassert_equal(test_hsm.num_msg_received, 1);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_GET_CURRENT_STATE);
}

ZTEST(hsm_basic, test_transition_on_entry_start_double)
{
	hsm_start(&test_hsm.hsm, test_trans_on_entry_to_trans_on_entry_child);

	// We expect 7 msgs:
	// top_state: ENTRY
	// trans_on_entry_to_trans_on_entry_child: ENTRY,EXIT
	// trans_on_entry_to_start: ENTRY,EXIT
	// sub_state, start_state: ENTRY
	zassert_equal(test_hsm.num_msg_received, 7);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_top_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, HSM_MSG_ID_ENTRY);
	zassert_equal(test_hsm.msg_rx_data[1].state_fn,
		      test_trans_on_entry_to_trans_on_entry_child);
	zassert_equal(test_hsm.msg_rx_data[1].msg_id, HSM_MSG_ID_ENTRY);
	zassert_equal(test_hsm.msg_rx_data[2].state_fn,
		      test_trans_on_entry_to_trans_on_entry_child);
	zassert_equal(test_hsm.msg_rx_data[2].msg_id, HSM_MSG_ID_EXIT);
	zassert_equal(test_hsm.msg_rx_data[3].state_fn, test_trans_on_entry_to_start);
	zassert_equal(test_hsm.msg_rx_data[3].msg_id, HSM_MSG_ID_ENTRY);
	zassert_equal(test_hsm.msg_rx_data[4].state_fn, test_trans_on_entry_to_start);
	zassert_equal(test_hsm.msg_rx_data[4].msg_id, HSM_MSG_ID_EXIT);
	zassert_equal(test_hsm.msg_rx_data[5].state_fn, test_sub_state);
	zassert_equal(test_hsm.msg_rx_data[5].msg_id, HSM_MSG_ID_ENTRY);
	zassert_equal(test_hsm.msg_rx_data[6].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[6].msg_id, HSM_MSG_ID_ENTRY);

	test_hsm.num_msg_received = 0;
	publish_msg(&test_hsm.hsm, MSG_ID_TEST_GET_CURRENT_STATE);
	zassert_equal(test_hsm.num_msg_received, 1);
	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_start_state);
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
	hsm_start(&test_hsm.hsm, test_recursive_state);
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

ZTEST_SUITE(hsm_basic, NULL, NULL, before_test, NULL, NULL);