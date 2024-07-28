# Delayable Message

A publish subscribe example demonstrating the use of a delayable message to handle time based events.
The code for this tutorial is located in the
[samples/tutorials/delayable_msg](../../samples/tutorials/delayable_msg) directory. This tutorial is
an extension to the [The Basics](the_basics.md) tutorial where we want to extend it to flash the LED
while the button is held down. The tutorial assumes we are starting from where
[The Basics](the_basics.md) left off and we are making modifications to the existing code.

## LED Modifications

To flash the LED we need to get a message that time has passed into the LED subscriber so that it
can toggle the LED state at the correct time. To achieve this we will add a delayable message to
the led subscriber component and configure it so it is published to the subscriber at the correct
time.

### Message Identifier

First we have to define a time period between LED toggles and a message identifier for the delayable
message. Add the following into the [led.c](../../samples/tutorials/delayable_msg/src/led.c) file:

``` C
#define LED_FLASH_TIME K_MSEC(500)

enum priv_msg_id {
    PRIV_MSG_ID_TIMER = PUB_MSG_ID_MAX_ID + 1,
};
```

where `LED_FLASH_TIME` defines the time between LED toggles and `PRIV_MSG_ID_TIMER` defines a
private message identifier for the delayable message we are going to add.

### Delayable Message Definition

We then need to add a delayable message into the `led_subscriber` struct. We don't need any data in
the message so we can use the `struct pub_sub_timer_msg` message as our delayable message. Modify
the `led_subscriber` struct as follows:

``` C
struct led_subscriber {
    PUB_SUB_ADD_SUBSCRIBER_CMPNT(PUB_MSG_ID_MAX_ID);
    struct pub_sub_timer_msg timer_msg;
    const struct device *pwm_leds;
    bool is_led_on;
};
```

where `timer_msg` is our new delayable message and `is_led_on` is a boolean to track the current led
state.

### Delayable Message Initialization

The `timer_msg` needs to be initialized, modify the `g_led0_subscriber` initialization as follows:

``` C
static struct led_subscriber g_led0_subscriber = {
    PUB_SUB_INIT_SUBSCRIBER_CMPNT(g_led0_subscriber, &k_sys_work_q, led_msg_handler,
                        PUB_MSG_ID_MAX_ID),
    .timer_msg = PUB_SUB_TIMER_MSG_INITIALIZER(PRIV_MSG_ID_TIMER,
                            PUB_SUB_SUBSCRIBER_CMPNT(&g_led0_subscriber)),
    .pwm_leds = DEVICE_DT_GET(LED_PWM_NODE_ID),
    .is_led_on = false,
};
```

where `PUB_SUB_TIMER_MSG_INITIALIZER(PRIV_MSG_ID_TIMER, PUB_SUB_SUBSCRIBER_CMPNT(&g_led0_subscriber))`
sets the `timer_msg` message identifier to `PRIV_MSG_ID_TIMER` and sets the subscriber it is going to
publish to on timeout to `g_led0_subscriber`.

### Set LED function

We want to set the LED state from a couple of different places in the subscriber message handler so
add the following helper function to set the led state and update the `led_subscriber`'s `is_led_on`
state:

``` C
static void set_led(struct led_subscriber *led_sub, bool turn_on)
{
    if (turn_on) {
        led_on(led_sub->pwm_leds, GREEN_LED_CHANNEL);
        LOG_INF("LED on");
    } else {
        led_off(led_sub->pwm_leds, GREEN_LED_CHANNEL);
        LOG_INF("LED off");
    }
    led_sub->is_led_on = turn_on;
}
```

### Handle button message

We then need to modify the button message handler to start the delayable message if the button is
down and stop it if the button is up. Modify the button message's case statement as follows:

``` C
case PUB_MSG_ID_BUTTON_STATE: {
    const struct button_state_msg *button_state_msg = msg;
    set_led(led_sub, button_state_msg->button_is_down);
    if (button_state_msg->button_is_down) {
        pub_sub_delayable_msg_start(
            PUB_SUB_DECLARED_TO_DELAYABLE_MSG(&led_sub->timer_msg),
            LED_FLASH_TIME);
    } else {
        pub_sub_delayable_msg_abort(
            PUB_SUB_DECLARED_TO_DELAYABLE_MSG(&led_sub->timer_msg));
    }
    return;
}
```

Where:

* `set_led` is called to set the LED to the current button state
* `pub_sub_delayable_msg_start` is used to start the delayable message with a timeout equal to
the `LED_FLASH_TIME` defined previously. `PUB_SUB_DECLARED_TO_DELAYABLE_MSG` is used to cast the
`timer_msg` to a generic `struct pub_sub_delayable_msg` type.
* `pub_sub_delayable_msg_abort` is used to stop `timer_msg` if the button is no longer down

### Handle delayable message

We then need to handle the delayable message when it is published to the subscriber. Add the follow
case statement to the led subscriber's message handler function:

``` C
case PRIV_MSG_ID_TIMER: {
    struct pub_sub_delayable_msg *delayable_msg =
        PUB_SUB_MSG_TO_DELAYABLE_MSG(struct pub_sub_delayable_msg, msg);
    if (pub_sub_delayable_msg_was_aborted(delayable_msg)) {
        return;
    }
    set_led(led_sub, !led_sub->is_led_on);
    pub_sub_delayable_msg_start_from_last(delayable_msg, LED_FLASH_TIME);
    return;
}
```

Where:

* `PUB_SUB_MSG_TO_DELAYABLE_MSG` is used to cast the received `void *msg` to a
`struct pub_sub_delayable_msg`
* `pub_sub_delayable_msg_was_aborted` is used to check that the message wasn't aborted while it was
queued with the subscriber
* `set_led` is used to toggle the led state
* `pub_sub_delayable_msg_start_from_last` is used to start the delayable message with a timeout
`LED_FLASH_TIME` from the previous timeout's end time.

#### pub_sub_delayable_msg_was_aborted

It is generally good practice to check if a delayable message was aborted before acting on it. This
is because there is a window of time between when the delayable message gets published to the
subscriber and when the subscriber's message handler function is called to process the message. If
the delayable message is aborted or restarted during this window the queued message should be ignored.

Using this LED subscriber as an example it is possible for the button to be released just before the
delayable message times out such that the subscriber's message queue is:
`[PUB_MSG_ID_BUTTON_STATE, PRIV_MSG_ID_TIMER]`. The button state message gets handled first and the
`button_is_down` value is false causing the LED to be turned off and the delayable message to be
aborted. However, `PRIV_MSG_ID_TIMER` is already queued with the subscriber so the handler function
is called for that message next. If the aborted state of the message is not checked then the handler
will turn the LED on and restart the delayable message timeout for the next LED flash. This leaves
the LED in the flashing state even though the button is released which is not the behavior we want.

#### delayable_msg_start vs delayable_msg_start_from_last

The difference between `pub_sub_delayable_msg_start` and `pub_sub_delayable_msg_start_from_last` is
when the timeout is calculated from. `start_from_last` calculates the end time of the timeout from
the previous timeout's end time whereas `start` calculates the end time from the current system tick.
It is not really important for this tutorial i.e. whether the LED toggles every 500 milliseconds or
every 500.1 milliseconds doesn't really matter it is just used to demonstrate the functionality.
