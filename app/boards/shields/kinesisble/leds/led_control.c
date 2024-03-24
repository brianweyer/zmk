/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/__assert.h>
#include <string.h>


#include <zephyr/bluetooth/services/bas.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/usb.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>

/* size of stack area used by each thread */
#define STACKSIZE 1024

/* scheduling priority used by each thread */
#define PRIORITY 7

#define LED_1_NODE DT_NODELABEL(ledu1)
#define LED_2_NODE DT_NODELABEL(ledu2)
#define LED_3_NODE DT_NODELABEL(ledu3)
#define LED_4_NODE DT_NODELABEL(ledu4)

struct led {
    const struct device *gpio_dev;
    const char *gpio_pin_name;
    unsigned int gpio_pin;
    unsigned int gpio_flags;
};

struct led_data_t {
    void *fifo_reserved;
    uint32_t index;
};

K_FIFO_DEFINE(led_fifo);

static void blink(const struct led *, uint32_t);
void send_display_battery(void);
void send_display_value(uint8_t value);

enum { LED_CAP, LED_NUM, LED_SCR, LED_KEY };
struct led leds[] = {[LED_CAP] =
                         {
                             .gpio_dev = DEVICE_DT_GET(DT_GPIO_CTLR(LED_1_NODE, gpios)),
                             .gpio_pin_name = DT_PROP(LED_1_NODE, label),
                             .gpio_pin = DT_GPIO_PIN(LED_1_NODE, gpios),
                             .gpio_flags = GPIO_OUTPUT | DT_GPIO_FLAGS(LED_1_NODE, gpios),
                         },
                     [LED_NUM] =
                         {
                             .gpio_dev = DEVICE_DT_GET(DT_GPIO_CTLR(LED_2_NODE, gpios)),
                             .gpio_pin_name = DT_PROP(LED_2_NODE, label),
                             .gpio_pin = DT_GPIO_PIN(LED_2_NODE, gpios),
                             .gpio_flags = GPIO_OUTPUT | DT_GPIO_FLAGS(LED_2_NODE, gpios),
                         },
                     [LED_SCR] =
                         {
                             .gpio_dev = DEVICE_DT_GET(DT_GPIO_CTLR(LED_3_NODE, gpios)),
                             .gpio_pin_name = DT_PROP(LED_3_NODE, label),
                             .gpio_pin = DT_GPIO_PIN(LED_3_NODE, gpios),
                             .gpio_flags = GPIO_OUTPUT | DT_GPIO_FLAGS(LED_3_NODE, gpios),
                         },
                     [LED_KEY] = {
                         .gpio_dev = DEVICE_DT_GET(DT_GPIO_CTLR(LED_4_NODE, gpios)),
                         .gpio_pin_name = DT_PROP(LED_4_NODE, label),
                         .gpio_pin = DT_GPIO_PIN(LED_4_NODE, gpios),
                         .gpio_flags = GPIO_OUTPUT | DT_GPIO_FLAGS(LED_4_NODE, gpios),
                     }};

static int led_init(const struct device *dev) {
    for (int i = 0; i < (sizeof(leds) / sizeof(struct led)); i++) {
        leds[i].gpio_dev = device_get_binding(leds[i].gpio_dev->name);
        if (leds[i].gpio_dev == NULL) {
            printk("Error: didn't find %s device\n", leds[i].gpio_dev->name);
            return -EIO;
        };

        int ret = gpio_pin_configure(leds[i].gpio_dev, leds[i].gpio_pin, leds[i].gpio_flags);
        if (ret != 0) {
            printk("Error %d: failed to configure pin %d '%s'\n", ret, leds[i].gpio_pin,
                   leds[i].gpio_pin_name);
            return -EIO;
        }
    }
    return 1;
}

SYS_INIT(led_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

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
            blink(&leds[0], BATTERY_LED_SLEEP_PERIOD*40);
            k_msleep(BATTERY_LED_SLEEP_PERIOD);
        }
    } else {
        ledON(&leds[0]);
        k_msleep(BATTERY_LED_SLEEP_PERIOD);
        if (level > 20) {
            ledON(&leds[1]);
            k_msleep(BATTERY_LED_SLEEP_PERIOD);
        }
        if (level > 40) {
            ledON(&leds[2]);
            k_msleep(BATTERY_LED_SLEEP_PERIOD);
        }
        if (level > 80) {
            ledON(&leds[3]);
            k_msleep(BATTERY_LED_SLEEP_PERIOD);
        }
        k_msleep(BATTERY_LED_SLEEP_PERIOD*40);
    }
    led_all_OFF();
}

#define LEVEL_LED_SLEEP_PERIOD 500

void display_value(uint8_t value) {
    if (value > 3) {
        ledON(&leds[3]);
        value -= 4;
    }
    if (value > 2) {
        ledON(&leds[2]);
        value -= 3;
    }
    if (value > 1) {
        ledON(&leds[1]);
        value -= 2;
    }
    if (value > 0) {
        ledON(&leds[0]);
    }
    k_msleep(LEVEL_LED_SLEEP_PERIOD);
    led_all_OFF();
};

void send_display_value(uint8_t index) {
    struct led_data_t tx_data = { .index = index };

    size_t size = sizeof(struct led_data_t);
    char *mem_ptr = k_malloc(size);
    __ASSERT_NO_MSG(mem_ptr != 0);

    memcpy(mem_ptr, &tx_data, size);

    k_fifo_put(&led_fifo, mem_ptr);
}

void send_display_battery() {
    struct led_data_t tx_data = {};

    size_t size = sizeof(struct led_data_t);
    char *mem_ptr = k_malloc(size);
    __ASSERT_NO_MSG(mem_ptr != 0);

    memcpy(mem_ptr, &tx_data, size);

    k_fifo_put(&led_fifo, mem_ptr);
}

void led_watcher(void)
{
	while (1) {
		struct led_data_t *rx_data = k_fifo_get(&led_fifo,
							   K_FOREVER);

        if (rx_data->index) {
            printk("Toggled led with index %d;",
		        rx_data->index);
            display_value(rx_data->index);
        } else {
            display_battery();
        }
		k_free(rx_data);
	}
}

K_THREAD_DEFINE(led_watcher_id, STACKSIZE, led_watcher, NULL, NULL, NULL,
		PRIORITY, 0, 0);