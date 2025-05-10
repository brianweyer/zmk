/*
 * Copyright (c) 2021 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_battery_status

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <dt-bindings/zmk/battery_status.h>
#include <zmk/backlight.h>
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
static const struct device *const battery_status_dev = DEVICE_DT_GET(DT_CHOSEN(zmk_battery_status));
#define ZMK_BATTERY_STATUS_LEDS DT_CHOSEN(zmk_battery_status)
#define CHILD_COUNT(...) +1
#define DT_NUM_CHILD(node_id) (DT_FOREACH_CHILD(node_id, CHILD_COUNT))

#define BATTERY_STATUS_NUM_LEDS (DT_NUM_CHILD(DT_CHOSEN(zmk_battery_status)))

struct led {
    const struct device *gpio_dev;
    const char *gpio_pin_name;
    unsigned int gpio_pin;
    unsigned int gpio_flags;
};


struct led leds[BATTERY_STATUS_NUM_LEDS] = {};


static int behavior_battery_status_init(const struct device *dev) { 
    // for (int i = 0; i < BATTERY_STATUS_NUM_LEDS; i++) {

    //     leds[i].gpio_dev = DEVICE_DT_GET(DT_GPIO_CTLR(i, gpios));
    //     leds[i].gpio_pin_name = DT_PROP(i, label);
    //     leds[i].gpio_pin = DT_GPIO_PIN(i, gpios);
    //     leds[i].gpio_flags = GPIO_OUTPUT | DT_GPIO_FLAGS(i, gpios);
    // }

    return 0; 
}

// static int led_init(const struct device *dev) {
//     for (int i = 0; i < (sizeof(leds) / sizeof(struct led)); i++) {
//         leds[i].gpio_dev = device_get_binding(leds[i].gpio_dev->name);
//         if (leds[i].gpio_dev == NULL) {
//             printk("Error: didn't find %s device\n", leds[i].gpio_dev->name);
//             return -EIO;
//         };

//         int ret = gpio_pin_configure(leds[i].gpio_dev, leds[i].gpio_pin, leds[i].gpio_flags);
//         if (ret != 0) {
//             printk("Error %d: failed to configure pin %d '%s'\n", ret, leds[i].gpio_pin,
//                    leds[i].gpio_pin_name);
//             return -EIO;
//         }
//     }
//     return 1;
// }

// SYS_INIT(led_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

void blink(const struct led *led, uint32_t sleep_ms) {
    gpio_pin_set(led->gpio_dev, led->gpio_pin, 1);
    k_msleep(sleep_ms);
    gpio_pin_set(led->gpio_dev, led->gpio_pin, 0);
}

static inline void ledON(const struct led *led) { gpio_pin_set(led->gpio_dev, led->gpio_pin, 1); }

static inline void ledOFF(const struct led *led) { gpio_pin_set(led->gpio_dev, led->gpio_pin, 0); }

static void led_all_OFF() {
    for (int i = 0; i < (sizeof(leds) / sizeof(struct led)); i++) {
        gpio_pin_set(leds[i].gpio_dev, leds[i].gpio_pin, 0);
    }
};

#define BATTERY_LED_SLEEP_PERIOD 100

void display_battery(void) {
    uint8_t level = bt_bas_get_battery_level();
    if (level <= 10) {
        for (int i = 0; i < 5; i++) {
            blink(&leds[0], BATTERY_LED_SLEEP_PERIOD);
            k_msleep(BATTERY_LED_SLEEP_PERIOD);
        }
    } else {
        ledON(&leds[0]);
        k_msleep(BATTERY_LED_SLEEP_PERIOD);
        for (int i = 0; i < (sizeof(leds)); i++) {
            if (level > ((i/(sizeof(leds))*100))) {
                ledON(&leds[i+1]);
                k_msleep(BATTERY_LED_SLEEP_PERIOD);
            }
        }
        k_msleep(BATTERY_LED_SLEEP_PERIOD);
    }
    led_all_OFF();
}

static int
on_keymap_binding_convert_central_state_dependent_params(struct zmk_behavior_binding *binding,
                                                         struct zmk_behavior_binding_event event) {
    return 0;
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    display_battery()

    return 0;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_battery_status_api = {
    .binding_convert_central_state_dependent_params =
        on_keymap_binding_convert_central_state_dependent_params,
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
};

DEVICE_DT_INST_DEFINE(0, behavior_battery_status_init, NULL, NULL, NULL, APPLICATION,
                      CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_battery_status_api);

static bool prior_state_was_sleep = false;

int userled_event_listener(const zmk_event_t *eh) {
    struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);
    if (ev == NULL) {
        return -ENOTSUP;
    }

    switch (ev->state) {
    case ZMK_ACTIVITY_ACTIVE:
        if (prior_state_was_sleep)
            display_battery();
        prior_state_was_sleep = false;
        break;
    case ZMK_ACTIVITY_SLEEP:
        prior_state_was_sleep = true;
        break;
    case ZMK_ACTIVITY_IDLE:
        prior_state_was_sleep = false;
        break;
    default:
        LOG_WRN("Unhandled activity state: %d", ev->state);
        return -EINVAL;
    }
    return 0;
}

ZMK_LISTENER(userled, userled_event_listener);
ZMK_SUBSCRIPTION(userled, zmk_activity_state_changed);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */