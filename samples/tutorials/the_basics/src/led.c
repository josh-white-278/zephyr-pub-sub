#include <pub_sub/pub_sub.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include "public_msgs.h"

LOG_MODULE_REGISTER(led);

#define LED0_NODE DT_ALIAS(led0)
#if !DT_NODE_HAS_STATUS(LED0_NODE, okay)
#error "Unsupported board: led0 devicetree alias is not defined"
#endif

struct led_subscriber {
	PUB_SUB_ADD_SUBSCRIBER_CMPNT(PUB_MSG_ID_MAX_ID);
	const struct gpio_dt_spec *led_gpio;
};

static void led_msg_handler(struct pub_sub_subscriber *subscriber, uint16_t msg_id,
			    const void *msg);

static const struct gpio_dt_spec g_led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static struct led_subscriber g_led0_subscriber = {
	PUB_SUB_INIT_SUBSCRIBER_CMPNT(g_led0_subscriber, &k_sys_work_q, led_msg_handler,
				      PUB_MSG_ID_MAX_ID),
	.led_gpio = &g_led0,
};
PUB_SUB_SUBSCRIBER_ADD(PUB_SUB_SUBSCRIBER_CMPNT(&g_led0_subscriber), 0);

static void led_msg_handler(struct pub_sub_subscriber *subscriber, uint16_t msg_id, const void *msg)
{
	struct led_subscriber *led_sub =
		PUB_SUB_CONTAINER_FROM_SUBSCRIBER(subscriber, struct led_subscriber);
	switch (msg_id) {
	case PUB_MSG_ID_BUTTON_STATE: {
		const struct button_state_msg *button_state_msg = msg;
		// state == 1 led on
		// state == 0 led off
		int state = button_state_msg->button_is_down ? 1 : 0;
		gpio_pin_set_dt(led_sub->led_gpio, state);
		LOG_INF("LED state: %d", state);
		return;
	}
	}
}

static int init_led(void)
{
	if (!gpio_is_ready_dt(&g_led0)) {
		LOG_ERR("LED device is not ready");
	}

	int ret = gpio_pin_configure_dt(&g_led0, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		LOG_ERR("LED configuration failed: %d", ret);
	}

	gpio_pin_set_dt(&g_led0, 0);

	pub_sub_subscribe(PUB_SUB_SUBSCRIBER_CMPNT(&g_led0_subscriber), PUB_MSG_ID_BUTTON_STATE);
	return 0;
}

SYS_INIT(init_led, APPLICATION, 0);