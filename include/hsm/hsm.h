/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef HSM_HSM_H_
#define HSM_HSM_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <pub_sub/pub_sub.h>
#include <stdint.h>

// Reserve highest msg ids for HSM private messages
enum hsm_msg_id {
	// Walk should never be handled by a state machine, it
	// is used internally to find the parent states of a state
	HSM_MSG_ID_WALK = UINT16_MAX,
	// Entry is published to a state when it is entered
	HSM_MSG_ID_ENTRY = HSM_MSG_ID_WALK - 1,
	// Exit is published to a state when it is exited
	HSM_MSG_ID_EXIT = HSM_MSG_ID_ENTRY - 1,
};

// Do not use these return codes directly use the corresponding
// macros defined below.
enum hsm_ret {
	HSM_RET_CONSUMED,
	HSM_RET_PARENT,
	HSM_RET_TOP_STATE,
	HSM_RET_TRANSITION,
};

// Returned when a message is consumed by the current state
#define HSM_CONSUMED()          (HSM_RET_CONSUMED)
// Parent or top state should be the default return value of a state
#define HSM_PARENT(_parent_fn)  (hsm->tmp_state = _parent_fn, HSM_RET_PARENT)
#define HSM_TOP_STATE()         (hsm->tmp_state = NULL, HSM_RET_TOP_STATE)
// Returned to transition the state machine from the current state to a new state
#define HSM_TRANSITION(_new_fn) (hsm->tmp_state = _new_fn, HSM_RET_TRANSITION)

struct hsm;
typedef enum hsm_ret (*hsm_state_fn)(struct hsm *hsm, uint16_t msg_id, const void *msg);

struct hsm {
	hsm_state_fn current_state;
	hsm_state_fn tmp_state;
	struct pub_sub_subscriber subscriber;
};

/**
 * @brief Initialize an HSM
 *
 * Initializing an HSM sets its starting state function and sets its subscriber's message handler
 * to the HSM framework's message handler. The HSM's subscriber must be initialized independently
 * from and in addition to the initialization of the HSM.
 *
 * @param hsm Address of the HSM
 * @param initial_state The HSM's starting state
 */
void hsm_init(struct hsm *hsm, hsm_state_fn initial_state);

/**
 * @brief Start an HSM
 *
 * Starting an HSM sends entry messages to the initial state and its parent states.The HSM must be
 * initialized before it can be started.
 *
 * @param hsm Address of the HSM
 */
void hsm_start(struct hsm *hsm);

#ifdef __cplusplus
}
#endif

#endif /* HSM_HSM_H_ */