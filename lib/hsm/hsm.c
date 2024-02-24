/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <hsm/hsm.h>
#include <sys/types.h>

static void msg_handler(uint16_t msg_id, const void *msg, void *user_data);
static void transition_state(struct hsm *hsm, hsm_state_fn new_state);

void hsm_init(struct hsm *hsm, hsm_state_fn initial_state)
{
	__ASSERT(hsm != NULL, "");
	__ASSERT(initial_state != NULL, "");
	hsm->current_state = initial_state;

	pub_sub_subscriber_set_handler_data(&hsm->subscriber, msg_handler, hsm);
}

void hsm_start(struct hsm *hsm)
{
	__ASSERT(hsm != NULL, "");
	__ASSERT(hsm->current_state != NULL, "");
	// To start we want to send an ENTRY message to the current state and all of its parents
	enum hsm_ret ret;
	hsm_state_fn parents[CONFIG_HSM_MAX_NESTED_STATES];
	ssize_t num_parents = -1;
	hsm->tmp_state = hsm->current_state;
	// Collect all of the parents
	do {
		__ASSERT(hsm->tmp_state != NULL, "");
		ret = hsm->tmp_state(hsm, HSM_MSG_ID_WALK, NULL);
		__ASSERT((ret == HSM_RET_PARENT) || (ret == HSM_RET_TOP_STATE),
			 "Walk message must be ignored");
		num_parents++;
		parents[num_parents] = hsm->tmp_state;
	} while ((ret == HSM_RET_PARENT) && (num_parents < (ARRAY_SIZE(parents) - 1)));

	// Iterate down through the parents so the top states get the ENTRY message first
	for (ssize_t i = num_parents - 1; i >= 0; i--) {
		// Ignore the return, transitions are not allowed from ENTRY and we don't care if
		// each state consumes the message or not
		__ASSERT(parents[i] != NULL, "");
		(void)parents[i](hsm, HSM_MSG_ID_ENTRY, NULL);
	}
	// Finally, send an ENTRY to the current state
	(void)hsm->current_state(hsm, HSM_MSG_ID_ENTRY, NULL);
}

static void msg_handler(uint16_t msg_id, const void *msg, void *user_data)
{
	enum hsm_ret ret;
	struct hsm *hsm = (struct hsm *)user_data;
	hsm->tmp_state = hsm->current_state;
	do {
		__ASSERT(hsm->tmp_state != NULL, "");
		ret = hsm->tmp_state(hsm, msg_id, msg);
	} while (ret == HSM_RET_PARENT);

	if ((ret == HSM_RET_TRANSITION) && (hsm->tmp_state != hsm->current_state)) {
		__ASSERT(hsm->tmp_state != NULL, "");
		transition_state(hsm, hsm->tmp_state);
	}
}

static void transition_state(struct hsm *hsm, hsm_state_fn new_state)
{
	enum hsm_ret ret;
	ssize_t common_parent_index = -1;
	hsm_state_fn parents[CONFIG_HSM_MAX_NESTED_STATES];
	ssize_t new_state_num_parents = -1;

	// Collect the new state's parents
	hsm->tmp_state = new_state;
	do {
		__ASSERT(hsm->tmp_state != NULL, "");
		ret = hsm->tmp_state(hsm, HSM_MSG_ID_WALK, NULL);
		__ASSERT((ret == HSM_RET_PARENT) || (ret == HSM_RET_TOP_STATE),
			 "Walk message must be ignored");
		new_state_num_parents++;
		// Check if the current state is a parent of the new state
		if (hsm->tmp_state == hsm->current_state) {
			common_parent_index = new_state_num_parents;
		}
		parents[new_state_num_parents] = hsm->tmp_state;
	} while ((ret == HSM_RET_PARENT) && (common_parent_index == -1) &&
		 (new_state_num_parents < (ARRAY_SIZE(parents) - 1)));

	// If common_parent_index is still -1 then the current state is not a parent of the new
	// state. Therefore we need to send EXIT messages to the current state and all of its
	// parents until we find a common parent or run out of parents
	if (common_parent_index == -1) {
		size_t nest_count = 0;
		hsm->tmp_state = hsm->current_state;
		do {
			__ASSERT(hsm->tmp_state != NULL, "");
			ret = hsm->tmp_state(hsm, HSM_MSG_ID_EXIT, NULL);
			__ASSERT(ret != HSM_RET_TRANSITION, "Can not transition from exit");
			// if the current state returns CONSUMED then send a WALK to get the parent
			// state
			if (ret == HSM_RET_CONSUMED) {
				__ASSERT(hsm->tmp_state != NULL, "");
				ret = hsm->tmp_state(hsm, HSM_MSG_ID_WALK, NULL);
				__ASSERT((ret == HSM_RET_PARENT) || (ret == HSM_RET_TOP_STATE),
					 "Walk message must be ignored");
			}
			// Check all of the new state's parents to see if the current parent is
			// common
			if (ret == HSM_RET_PARENT) {
				for (size_t i = 0; i < new_state_num_parents; i++) {
					if (parents[i] == hsm->tmp_state) {
						common_parent_index = i;
					}
				}
			}
			nest_count++;
			// Once we hit either a non-parent return or a common parent then we can
			// stop sending EXIT messages
		} while ((ret == HSM_RET_PARENT) && (common_parent_index == -1) &&
			 (nest_count < CONFIG_HSM_MAX_NESTED_STATES));
	}

	// If common_parent_index is still -1 then there is no common parent so just send ENTRY to
	// all of the new state's parents
	if (common_parent_index == -1) {
		common_parent_index = new_state_num_parents;
	}

	// We then need to iterate down from the common parent sending an ENTRY message to each
	// finishing with an ENTRY to the new_state. -1 from the common_parent_index because
	// we don't want to send an ENTRY to the common parent as the state machine is already in
	// that parent state.
	for (ssize_t i = common_parent_index - 1; i >= 0; i--) {
		// Ignore the return, transitions are not allowed from ENTRY and we don't care if
		// each state consumes the message or not
		__ASSERT(parents[i] != NULL, "");
		(void)parents[i](hsm, HSM_MSG_ID_ENTRY, NULL);
	}
	__ASSERT(new_state != NULL, "");
	(void)new_state(hsm, HSM_MSG_ID_ENTRY, NULL);

	// Finally, update the current state to be the new state
	hsm->current_state = new_state;
}