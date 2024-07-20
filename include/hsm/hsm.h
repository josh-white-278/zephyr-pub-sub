/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef HSM_HSM_H_
#define HSM_HSM_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include <zephyr/sys/__assert.h>

struct hsm;
struct hsm_state;
typedef uintptr_t hsm_status_t;

// Reserve highest msg ids for HSM private messages
enum hsm_msg_id {
	// Entry is published to a state when it is entered
	HSM_MSG_ID_ENTRY = UINT16_MAX,
	// Exit is published to a state when it is exited
	HSM_MSG_ID_EXIT = HSM_MSG_ID_ENTRY - 1,
};

// Do not use these return codes directly, use the corresponding
// macros defined below.
enum hsm_ret {
	HSM_RET_PASS,
	HSM_RET_CONSUMED,
	HSM_RET_TRANSITION,
};

// Similar to sflist, we store the return enum in the lowest 2 bits of the state pointer
#define HSM_STATUS_RET_MASK         0x3
#define HSM_STATUS_TO_STATE(status) ((const struct hsm_state *)(status & ~HSM_STATUS_RET_MASK))
#define HSM_STATUS_TO_RET(status)   ((enum hsm_ret)(status & HSM_STATUS_RET_MASK))

// Returned when a message should be passed to the state's parent
#define HSM_PASS()     ((hsm_status_t)HSM_RET_PASS)
// Returned when a message is consumed by the current state
#define HSM_CONSUMED() ((hsm_status_t)HSM_RET_CONSUMED)
// Returned to transition from the current state to a new state
#define HSM_TRANSITION(_new_state)                                                                 \
	({                                                                                         \
		__ASSERT(((uintptr_t)_new_state & HSM_STATUS_RET_MASK) == 0,                       \
			 "State pointers must be 4 byte aligned");                                 \
		(hsm_status_t)((uintptr_t)_new_state | HSM_RET_TRANSITION);                        \
	})

/** @brief The signature for an HSM state's message handler function
 *
 * @param hsm The hsm that received the message
 * @param msg_id The id of the message received by the HSM
 * @param msg The message received by the HSM
 */
typedef hsm_status_t (*hsm_state_fn)(struct hsm *hsm, uint16_t msg_id, const void *msg);

struct hsm {
	const struct hsm_state *current_state;
};

struct hsm_state {
	const hsm_state_fn state_fn;
	const struct hsm_state *parent;
} __aligned(4);

/** @brief Define an HSM state
 *
 * @param _name The name of the state
 * @param _state_fn The state's message handler function
 * @param _parent_state The state's parent, NULL if no parent
 */
#define HSM_STATE_DEFINE(_name, _state_fn, _parent_state)                                          \
	struct hsm_state _name = {.state_fn = _state_fn, .parent = _parent_state}

/**
 * @brief Start an HSM
 *
 * Starting an HSM sets the current state to the initial state and sends entry messages to the
 * initial state and its parent states (parent states first, initial state last).
 *
 * @param hsm Address of the HSM
 * @param initial_state The HSM's starting state
 */
void hsm_start(struct hsm *hsm, const struct hsm_state *initial_state);

/**
 * @brief Run a message through an HSM
 *
 * Running an HSM calls the current state function with the message and executes any behavior
 * specified by the return, e.g. state transitions etc.
 *
 * @param hsm Address of the HSM to run
 * @param msg_id The id of the message
 * @param msg A pointer to the message
 */
void hsm_run(struct hsm *hsm, uint16_t msg_id, const void *msg);

#ifdef __cplusplus
}
#endif

#endif /* HSM_HSM_H_ */