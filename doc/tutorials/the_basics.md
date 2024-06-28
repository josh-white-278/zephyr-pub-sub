# The Basics

A simple publish subscribe example linking pushing a button to an LED turning on via a published
message. The code for this tutorial is located in the
[samples/tutorials/the_basics](../../samples/tutorials/the_basics) directory. This tutorial
assumes you have Zephyr up and running i.e. that you have followed the Zephyr getting started guide
and you have an empty app running on your board e.g. a main function something like:

``` C
int main(void)
{
    while (1) {
        k_sleep(K_SECONDS(10));
    }
    return 0;
}
```

## Configuration

To use the publish subscribe framework it needs to be enabled via Kconfig. We also want to use a
button as an input and an LED as an output. Add the following to your prj.conf file to enable the
components we need:

``` conf
CONFIG_PUB_SUB=y
CONFIG_GPIO=y
CONFIG_PWM=y
CONFIG_LED=y
```

## Define a public message

We want to send a message that represents the button state so that the LED can mirror it. I.e. when
the button transitions from up to down the LED transitions from off to on and vice versa. To do this
we need to create a public message that represents the button state. Create a file called
[public_msgs.h](../../samples/tutorials/the_basics/src/public_msgs.h) and add the following to it:

``` C
enum pub_msg_id {
    PUB_MSG_ID_BUTTON_STATE,
    PUB_MSG_ID_MAX_ID = PUB_MSG_ID_BUTTON_STATE,
};

// PUB_MSG_ID_BUTTON_STATE
struct button_state_msg {
    bool button_is_down;
};
```

`PUB_MSG_ID_BUTTON_STATE` defines the message identifier for the button state message and
`struct button_state_msg` defines the message that will be sent with that message identifier.
`PUB_MSG_ID_MAX_ID` is useful when defining subscribers as a subscriber needs to know the largest
message identifier it can subscribe to.

## Button

Add a file called [button.c](../../samples/tutorials/the_basics/src/button.c) to the application,
this file will handle reading the button state and publishing the state via the `button_state_msg`
message.

### Button initialization

Add the following to initialize `sw0` as an interrupt driven input:

``` C
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

static const struct gpio_dt_spec g_button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, {0});
static struct gpio_callback g_button_cb_data;

static void button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    int state = gpio_pin_get_dt(&g_button);
    LOG_INF("Button state: %d", state);
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
```

Running this code should result in the following log messages when the button is pressed:

``` log
[00:00:07.644,805] <inf> button: Button state: 1
[00:00:07.977,081] <inf> button: Button state: 0
[00:00:09.410,430] <inf> button: Button state: 1
[00:00:10.184,936] <inf> button: Button state: 0
[00:00:11.363,281] <inf> button: Button state: 1
[00:00:12.963,958] <inf> button: Button state: 0
[00:00:14.669,281] <inf> button: Button state: 1
[00:00:15.308,959] <inf> button: Button state: 0
```

### Publishing button state

Now that we can detect the button state in the `button_callback` function we need to publish it in a
message.

#### Message allocator

First we need a message allocator to allocate the messages from, add the following to define a new
allocator:

``` C
PUB_SUB_MEM_SLAB_ALLOCATOR_DEFINE_STATIC(g_msg_alloc, sizeof(struct button_state_msg), 4);
```

This defines a `k_mem_slab` backed message allocator called `g_msg_alloc` that has a capacity of 4
messages the size of the `button_state_msg` struct.

#### Message allocation

Using the message allocator we need to allocate a message whenever the button state changes. Add the
following into the `button_callback` function:

``` C
struct button_state_msg *msg =
    pub_sub_new_msg(&g_msg_alloc, PUB_MSG_ID_BUTTON_STATE,
        sizeof(struct button_state_msg), K_NO_WAIT);
```

This allocates a new message from `g_msg_alloc` with a message identifier of
`PUB_MSG_ID_BUTTON_STATE` and a message size equal to the size of the `button_state_msg` struct.

#### Publish the state

Finally we need to set the button state in the message and publish it:

``` C
msg->button_is_down = state == 1;
pub_sub_publish(msg);
```

The function `pub_sub_publish` publishes the message to the default broker which will pass the
message to any subscribers that are subscribed to `PUB_MSG_ID_BUTTON_STATE` and returns the message
to `g_msg_alloc` when all of the subscribers are finished handling the message.

## LED

Add a file called [led.c](../../samples/tutorials/the_basics/src/led.c) to the application, this
file will handle receiving the `button_state_msg` message and setting the LED state to match the
button state.

### LED initialization

Add the following to the file:

``` C
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

```

This defines the node ids and channel required to control the green led.

### Receiving button state

Now that we can control the LED we need to get the button state so that we can turn the LED on when
the button is down and turn the LED off when the button is up.

#### Declare a subscriber

First we need to declare a subscriber, add the following struct declaration:

``` C
struct led_subscriber {
    PUB_SUB_ADD_SUBSCRIBER_CMPNT(PUB_MSG_ID_MAX_ID);
    const struct device *pwm_leds;
};
```

This declares the struct `led_subscriber` as a composition of a subscriber and an PWM LED device
where `PUB_MSG_ID_MAX_ID` is the maximum message identifier that the subscriber can subscribe to.

#### Define and add a subscriber

We then need to define an instance of `struct led_subscriber` and add its subscriber to the default
broker so that it can receive the `button_state_msg` message. Add the following:

``` C
static void led_msg_handler(struct pub_sub_subscriber *subscriber, uint16_t msg_id,
    const void *msg);

static struct led_subscriber g_led0_subscriber = {
    PUB_SUB_INIT_SUBSCRIBER_CMPNT(g_led0_subscriber, &k_sys_work_q, led_msg_handler,
        PUB_MSG_ID_MAX_ID),
    .pwm_leds = DEVICE_DT_GET(LED_PWM_NODE_ID),
};
PUB_SUB_SUBSCRIBER_ADD(PUB_SUB_SUBSCRIBER_CMPNT(&g_led0_subscriber), 0);
```

`PUB_SUB_INIT_SUBSCRIBER_CMPNT` initializes the subscriber component added to
`struct led_subscriber` by `PUB_SUB_ADD_SUBSCRIBER_CMPNT` above, where:

* `&k_sys_work_q` is the work queue the subscriber will run on (in this case the system work queue)
* `led_msg_handler` is the function that will be called from the work queue for each of the messages
the subscriber receives
* PUB_MSG_ID_MAX_ID is the maximum message identifier that the subscriber can subscribe to

`PUB_SUB_SUBSCRIBER_ADD` adds the subscriber to the default broker with a priority of 0.
`PUB_SUB_SUBSCRIBER_CMPNT` is used to retrieve a pointer to the subscriber component of the
composite `g_led0_subscriber`.

#### Subscribe to button message

We want the `g_led0_subscriber` subscriber to receive the `button_state_msg` message when it is
published so we need to subscribe to it. Add the following init function to subscribe to the message
with identifier `PUB_MSG_ID_BUTTON_STATE`:

``` C
static int init_led(void)
{
    pub_sub_subscribe(PUB_SUB_SUBSCRIBER_CMPNT(&g_led0_subscriber), PUB_MSG_ID_BUTTON_STATE);
    return 0;
}

SYS_INIT(init_led, APPLICATION, 0);
```

#### Handle the button message

Finally we need to define the `led_msg_handler` message handler function and handle the
`button_state_msg` message, add the following function:

``` C
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
```

`subscriber` is a pointer to the `led_subscriber`'s subscriber component so
`PUB_SUB_CONTAINER_FROM_SUBSCRIBER` is used to retrieve a pointer to the composite struct. `msg_id`
identifies the message which allows the generic `void *` message to be cast to the correct message
type. After we have cast the message we can then map the message's `button_is_down` state to the LED
state.

## Final result

Running the code should result in the following log messages when the button is pushed and the LED
should turn on when the button is down:

``` log
[00:00:14.612,243] <inf> button: Button state: 1
[00:00:14.612,304] <inf> led: LED on
[00:00:15.101,165] <inf> button: Button state: 0
[00:00:15.101,226] <inf> led: LED off
[00:00:16.200,164] <inf> button: Button state: 1
[00:00:16.200,256] <inf> led: LED on
[00:00:16.375,854] <inf> button: Button state: 0
[00:00:16.375,915] <inf> led: LED off
```
