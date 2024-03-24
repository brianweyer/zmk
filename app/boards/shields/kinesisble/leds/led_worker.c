
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/__assert.h>
#include <string.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/keymap.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/keycode_state_changed.h>


void send_display_battery(void);
void send_display_value(uint8_t value);

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static bool prior_state_was_sleep = false;

int userled_event_listener(const zmk_event_t *eh) {
    struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);
    if (ev == NULL) {
        return -ENOTSUP;
    }

    switch (ev->state) {
    case ZMK_ACTIVITY_ACTIVE:
        if (prior_state_was_sleep)
            send_display_battery();
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

int ble_event_listener(const zmk_event_t *eh) {
    struct zmk_ble_active_profile_changed *ev = as_zmk_ble_active_profile_changed(eh);
    if (ev == NULL) {
        return -ENOTSUP;
    }
    LOG_DBG("Setting BLE profile: %d", ev->index);
    send_display_value(ev->index + 1);
    return 0;
}

ZMK_LISTENER(ble, ble_event_listener);
ZMK_SUBSCRIPTION(ble, zmk_ble_active_profile_changed);

int layer_event_listener(const zmk_event_t *eh) {
    struct zmk_layer_state_changed *ev = as_zmk_layer_state_changed(eh);
    if (ev == NULL) {
        return -ENOTSUP;
    }
    LOG_DBG("*****zmk_layer_state_changed listener: %d %d", ev->layer, ev->state);
    if (ev->state) {
        send_display_value(ev->layer + 1);
    }
    return 0;
}

ZMK_LISTENER(layer, layer_event_listener);
ZMK_SUBSCRIPTION(layer, zmk_layer_state_changed);

static int hid_listener_keycode_pressed(const struct zmk_keycode_state_changed *ev) {
    if (ev->keycode == HID_USAGE_KEY_KEYBOARD_F24) {
        send_display_battery();
    }
    return 0;
}

int led_keycode_listener(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (ev) {
        if (ev->state) {
            hid_listener_keycode_pressed(ev);
        }
    }
    return 0;
}

ZMK_LISTENER(keycode, led_keycode_listener);
ZMK_SUBSCRIPTION(keycode, zmk_keycode_state_changed);