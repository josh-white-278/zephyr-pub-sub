/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <hsm/hsm_subscriber.h>
#include <pub_sub/msg_alloc_mem_slab.h>
#include <zephyr/ztest.h>
#include <stdlib.h>

enum msg_id {
	MSG_ID_PUBLIC_MSG,
	MSG_ID_MAX_PUB_ID = MSG_ID_PUBLIC_MSG,
	MSG_ID_TEST_GET_CURRENT_STATE,
	MSG_ID_TEST_START_RX,
	MSG_ID_TEST_TOP_STATE_RX,
	MSG_ID_TEST_TRANSITION_START_STATE,
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
	HSM_SUB_COMPOSE(MSG_ID_MAX_PUB_ID);
	struct msg_rx_data msg_rx_data[16];
	int num_msg_received;
};

PUB_SUB_MEM_SLAB_ALLOCATOR_DEFINE_STATIC(test_allocator, sizeof(struct transition_msg), 32);

struct test_hsm test_hsm = {
	HSM_SUB_INIT_COMPOSED(test_hsm, &k_sys_work_q, MSG_ID_MAX_PUB_ID, 0),
};

static enum hsm_ret test_top_state(struct hsm *hsm, uint16_t msg_id, const void *msg)
{
	struct test_hsm *test_hsm = HSM_SUB_CONTAINER_FROM_HSM(hsm, struct test_hsm);
	switch (msg_id) {
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

static enum hsm_ret test_start_state(struct hsm *hsm, uint16_t msg_id, const void *msg)
{
	struct test_hsm *test_hsm = HSM_SUB_CONTAINER_FROM_HSM(hsm, struct test_hsm);
	switch (msg_id) {
	case MSG_ID_PUBLIC_MSG:
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
		return HSM_PARENT(test_top_state);
	}
}

static enum hsm_ret test_other_state(struct hsm *hsm, uint16_t msg_id, const void *msg)
{
	struct test_hsm *test_hsm = HSM_SUB_CONTAINER_FROM_HSM(hsm, struct test_hsm);
	switch (msg_id) {
	case MSG_ID_PUBLIC_MSG:
	case MSG_ID_TEST_GET_CURRENT_STATE:
		test_hsm->msg_rx_data[test_hsm->num_msg_received].state_fn = test_other_state;
		test_hsm->msg_rx_data[test_hsm->num_msg_received++].msg_id = msg_id;
		return HSM_CONSUMED();
	default:
		return HSM_PARENT(test_top_state);
	}
}

void publish_msg(uint16_t msg_id)
{
	void *msg = pub_sub_new_msg(&test_allocator, msg_id, 0, K_NO_WAIT);
	zassert_not_null(msg);
	if (msg_id > MSG_ID_MAX_PUB_ID) {
		pub_sub_publish_to_subscriber(HSM_SUB_COMPOSED_SUBSCRIBER_PTR(test_hsm), msg);
	} else {
		pub_sub_publish(msg);
	}
	// Delay to allow HSM to run
	k_sleep(K_MSEC(1));
}

static void publish_transition_state(uint16_t msg_id, hsm_state_fn dest_state)
{
	struct transition_msg *msg =
		pub_sub_new_msg(&test_allocator, msg_id, sizeof(struct transition_msg), K_NO_WAIT);
	zassert_not_null(msg);
	msg->dest_state = dest_state;
	pub_sub_publish_to_subscriber(HSM_SUB_COMPOSED_SUBSCRIBER_PTR(test_hsm), msg);
	// Delay to allow HSM to run
	k_sleep(K_MSEC(1));
}

static void *suite_setup(void)
{
	pub_sub_subscribe(HSM_SUB_COMPOSED_SUBSCRIBER_PTR(test_hsm), MSG_ID_PUBLIC_MSG);
	pub_sub_add_subscriber(HSM_SUB_COMPOSED_SUBSCRIBER_PTR(test_hsm));
	hsm_start(HSM_SUB_COMPOSED_HSM_PTR(test_hsm), test_start_state);
	return NULL;
}

static void before_test(void *fixture)
{
	ARG_UNUSED(fixture);
	test_hsm.num_msg_received = 0;
}

ZTEST(hsm_subscriber, test_receive)
{
	publish_msg(MSG_ID_TEST_START_RX);
	publish_msg(MSG_ID_PUBLIC_MSG);
	publish_msg(MSG_ID_TEST_TOP_STATE_RX);

	zassert_equal(test_hsm.num_msg_received, 3);

	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_START_RX);
	zassert_equal(test_hsm.msg_rx_data[1].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[1].msg_id, MSG_ID_PUBLIC_MSG);
	zassert_equal(test_hsm.msg_rx_data[2].state_fn, test_top_state);
	zassert_equal(test_hsm.msg_rx_data[2].msg_id, MSG_ID_TEST_TOP_STATE_RX);

	// HSM_SUB_CONTAINER_FROM_SUBSCRIBER isn't used anywhere so test it here
	zassert_equal_ptr(HSM_SUB_CONTAINER_FROM_SUBSCRIBER(&test_hsm._hsm_subscriber.subscriber,
							    struct test_hsm),
			  &test_hsm);
}

ZTEST(hsm_subscriber, test_transition)
{
	publish_transition_state(MSG_ID_TEST_TRANSITION_START_STATE, test_other_state);
	publish_msg(MSG_ID_TEST_GET_CURRENT_STATE);
	publish_msg(MSG_ID_PUBLIC_MSG);
	publish_msg(MSG_ID_TEST_TOP_STATE_RX);

	zassert_equal(test_hsm.num_msg_received, 4);

	zassert_equal(test_hsm.msg_rx_data[0].state_fn, test_start_state);
	zassert_equal(test_hsm.msg_rx_data[0].msg_id, MSG_ID_TEST_TRANSITION_START_STATE);
	zassert_equal(test_hsm.msg_rx_data[1].state_fn, test_other_state);
	zassert_equal(test_hsm.msg_rx_data[1].msg_id, MSG_ID_TEST_GET_CURRENT_STATE);
	zassert_equal(test_hsm.msg_rx_data[2].state_fn, test_other_state);
	zassert_equal(test_hsm.msg_rx_data[2].msg_id, MSG_ID_PUBLIC_MSG);
	zassert_equal(test_hsm.msg_rx_data[3].state_fn, test_top_state);
	zassert_equal(test_hsm.msg_rx_data[3].msg_id, MSG_ID_TEST_TOP_STATE_RX);
}

ZTEST_SUITE(hsm_subscriber, NULL, suite_setup, before_test, NULL, NULL);