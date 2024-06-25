#ifndef PUBLIC_MSGS_H_
#define PUBLIC_MSGS_H_

#ifdef __cplusplus
extern "C" {
#endif

enum pub_msg_id {
	PUB_MSG_ID_BUTTON_STATE,
	PUB_MSG_ID_MAX_ID = PUB_MSG_ID_BUTTON_STATE,
};

// PUB_MSG_ID_BUTTON_STATE
struct button_state_msg {
	bool button_is_down;
};

#ifdef __cplusplus
}
#endif

#endif /* PUBLIC_MSGS_H_ */