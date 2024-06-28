#include <pub_sub/pub_sub.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include "public_msgs.h"

LOG_MODULE_REGISTER(led);

#define LED_PWM_NODE_ID   DT_PATH(pwmleds)
#define GREEN_LED_NODE_ID DT_NODELABEL(green_pwm_led)
#define GREEN_LED_CHANNEL DT_PHA_BY_IDX(GREEN_LED_NODE_ID, pwms, 0, channel)

#if !DT_NODE_HAS_STATUS(LED_PWM_NODE_ID, okay)
#error "Unsupported board: pwmleds devicetree node is not defined"
#endif

#if !DT_NODE_HAS_STATUS(GREEN_LED_NODE_ID, okay)
#error "Unsupported board: green_pwm_led devicetree node is not defined"
#endif

struct led_subscriber {
	PUB_SUB_ADD_SUBSCRIBER_CMPNT(PUB_MSG_ID_MAX_ID);
	const struct device *pwm_leds;
};

static void led_msg_handler(struct pub_sub_subscriber *subscriber, uint16_t msg_id,
			    const void *msg);

static struct led_subscriber g_led0_subscriber = {
	PUB_SUB_INIT_SUBSCRIBER_CMPNT(g_led0_subscriber, &k_sys_work_q, led_msg_handler,
				      PUB_MSG_ID_MAX_ID),
	.pwm_leds = DEVICE_DT_GET(LED_PWM_NODE_ID),
};
PUB_SUB_SUBSCRIBER_ADD(PUB_SUB_SUBSCRIBER_CMPNT(&g_led0_subscriber), 0);

static void led_msg_handler(struct pub_sub_subscriber *subscriber, uint16_t msg_id, const void *msg)
{
	struct led_subscriber *led_sub =
		PUB_SUB_CONTAINER_FROM_SUBSCRIBER(subscriber, struct led_subscriber);
	switch (msg_id) {
	case PUB_MSG_ID_BUTTON_STATE: {
		const struct button_state_msg *button_state_msg = msg;
		if (button_state_msg->button_is_down) {
			led_on(led_sub->pwm_leds, GREEN_LED_CHANNEL);
			LOG_INF("LED on");
		} else {
			led_off(led_sub->pwm_leds, GREEN_LED_CHANNEL);
			LOG_INF("LED off");
		}
		return;
	}
	}
}

static int init_led(void)
{
	pub_sub_subscribe(PUB_SUB_SUBSCRIBER_CMPNT(&g_led0_subscriber), PUB_MSG_ID_BUTTON_STATE);
	return 0;
}

SYS_INIT(init_led, APPLICATION, 0);