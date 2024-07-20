/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <hsm/hsm.h>
#include <sys/types.h>
#include <stddef.h>
#include <zephyr/kernel.h>

static hsm_status_t transition_state(struct hsm *hsm, const struct hsm_state *new_state);
static hsm_status_t null_state_fn(struct hsm *hsm, uint16_t msg_id, const void *msg);

static const HSM_STATE_DEFINE(null_state, null_state_fn, NULL);

void hsm_start(struct hsm *hsm, const struct hsm_state *initial_state)
{
	__ASSERT(hsm != NULL, "");
	__ASSERT(initial_state != NULL, "");
	__ASSERT(((uintptr_t)initial_state & HSM_STATUS_RET_MASK) == 0,
		 "State pointers must be 4 byte aligned");
	// To start the HSM we set the current state to the null state and then transition to the
	// initial state. The initial state and all of its parents will receive an entry message as
	// the null state will not be a parent of the initial state.
	hsm->current_state = &null_state;
	hsm_status_t status = transition_state(hsm, initial_state);
	while ((HSM_STATUS_TO_RET(status) == HSM_RET_TRANSITION) &&
	       (HSM_STATUS_TO_STATE(status) != hsm->current_state)) {
		__ASSERT(HSM_STATUS_TO_STATE(status) != NULL, "");
		status = transition_state(hsm, HSM_STATUS_TO_STATE(status));
	}
}

void hsm_run(struct hsm *hsm, uint16_t msg_id, const void *msg)
{
	hsm_status_t status;
	const struct hsm_state *tmp_state = hsm->current_state;
	__ASSERT(tmp_state != NULL, "");
	do {
		status = tmp_state->state_fn(hsm, msg_id, msg);
		tmp_state = tmp_state->parent;
	} while ((HSM_STATUS_TO_RET(status) == HSM_RET_PASS) && (tmp_state != NULL));

	while ((HSM_STATUS_TO_RET(status) == HSM_RET_TRANSITION) &&
	       (HSM_STATUS_TO_STATE(status) != hsm->current_state)) {
		__ASSERT(HSM_STATUS_TO_STATE(status) != NULL, "");
		status = transition_state(hsm, HSM_STATUS_TO_STATE(status));
	}
}

static hsm_status_t transition_state(struct hsm *hsm, const struct hsm_state *new_state)
{
	hsm_status_t status;
	ssize_t common_parent_index = -1;
	const struct hsm_state *parents[CONFIG_HSM_MAX_NESTED_STATES];
	ssize_t new_state_num_parents = 0;

	// Collect the new state's parents
	const struct hsm_state *tmp_state = new_state->parent;
	while ((tmp_state != NULL) && (new_state_num_parents < ARRAY_SIZE(parents))) {
		__ASSERT(((uintptr_t)tmp_state & HSM_STATUS_RET_MASK) == 0,
			 "State pointers must be 4 byte aligned");
		parents[new_state_num_parents] = tmp_state;
		new_state_num_parents++;
		// Check if the current state is a parent of the new state
		if (tmp_state == hsm->current_state) {
			common_parent_index = new_state_num_parents - 1;
			break;
		}
		tmp_state = tmp_state->parent;
	}

	// If common_parent_index is still -1 then the current state is not a parent of the new
	// state. Therefore we need to send EXIT messages to the current state and all of its
	// parents until we find a common parent or run out of parents
	if (common_parent_index == -1) {
		size_t nest_count = 0;
		tmp_state = hsm->current_state;
		do {
			// Exit the current state
			status = tmp_state->state_fn(hsm, HSM_MSG_ID_EXIT, NULL);
			__ASSERT(HSM_STATUS_TO_RET(status) != HSM_RET_TRANSITION,
				 "Can not transition from exit");
			// Move to the next parent
			tmp_state = tmp_state->parent;
			if (tmp_state == NULL) {
				break;
			}
			// Check all of the new state's parents to see if the next parent is common
			for (size_t i = 0; i < new_state_num_parents; i++) {
				if (parents[i] == tmp_state) {
					common_parent_index = i;
					break;
				}
			}
			nest_count++;
		} while ((common_parent_index < 0) &&
			 (nest_count < CONFIG_HSM_MAX_NESTED_STATES + 1));
	}

	// If common_parent_index is still -1 then there is no common parent so just send ENTRY to
	// all of the new state's parents
	if (common_parent_index < 0) {
		common_parent_index = new_state_num_parents;
	}

	// We then need to iterate down from the common parent sending an ENTRY message to each
	// finishing with an ENTRY to the new_state. -1 from the common_parent_index because
	// we don't want to send an ENTRY to the common parent as the state machine is already in
	// that parent state.
	for (ssize_t i = common_parent_index - 1; i >= 0; i--) {
		__ASSERT(parents[i] != NULL, "");
		status = parents[i]->state_fn(hsm, HSM_MSG_ID_ENTRY, NULL);
		if (HSM_STATUS_TO_RET(status) == HSM_RET_TRANSITION) {
			// Transitioning from an entry, the current state becomes the state we just
			// entered and we return early
			hsm->current_state = parents[i];
			return status;
		}
	}
	status = new_state->state_fn(hsm, HSM_MSG_ID_ENTRY, NULL);
	// Finally, update the current state to be the new state
	hsm->current_state = new_state;
	return status;
}

static hsm_status_t null_state_fn(struct hsm *hsm, uint16_t msg_id, const void *msg)
{
	return HSM_PASS();
}