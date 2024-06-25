#include <pub_sub/pub_sub.h>
#include <pub_sub/msg_alloc_mem_slab.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "public_msgs.h"

LOG_MODULE_REGISTER(button);

#define SW0_NODE DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif

static const struct gpio_dt_spec g_button = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
static struct gpio_callback g_button_cb_data;
PUB_SUB_MEM_SLAB_ALLOCATOR_DEFINE_STATIC(g_msg_alloc, sizeof(struct button_state_msg), 4);

static void button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	int state = gpio_pin_get_dt(&g_button);
	LOG_INF("Button state: %d", state);
	struct button_state_msg *msg = pub_sub_new_msg(&g_msg_alloc, PUB_MSG_ID_BUTTON_STATE,
						       sizeof(struct button_state_msg), K_NO_WAIT);
	if (msg != NULL) {
		// state == 1 button pressed
		// state == 0 button not pressed
		msg->button_is_down = state == 1;
		pub_sub_publish(msg);
	} else {
		LOG_ERR("NULL msg");
	}
}

static int init_button(void)
{
	if (!gpio_is_ready_dt(&g_button)) {
		LOG_ERR("Button not ready");
		return 0;
	}

	int ret = gpio_pin_configure_dt(&g_button, GPIO_INPUT);
	if (ret != 0) {
		LOG_ERR("Button failed to configure: %d", ret);
		return 0;
	}

	ret = gpio_pin_interrupt_configure_dt(&g_button, GPIO_INT_EDGE_BOTH);
	if (ret != 0) {
		LOG_ERR("Button failed to configure interrupt: %d", ret);
		return 0;
	}

	gpio_init_callback(&g_button_cb_data, button_callback, BIT(g_button.pin));
	ret = gpio_add_callback(g_button.port, &g_button_cb_data);
	if (ret != 0) {
		LOG_ERR("Button failed to add callback to interrupt: %d", ret);
		return 0;
	}
	return 0;
}

SYS_INIT(init_button, APPLICATION, 0);