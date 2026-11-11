/*
 * Flash the board's blue_led (profile index + 1) times when the active Bluetooth
 * profile changes: 1 flash for profile 0, 2 for profile 1, and so on.
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>

LOG_MODULE_REGISTER(ble_profile_led, CONFIG_ZMK_LOG_LEVEL);

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_NODELABEL(blue_led), gpios);

static struct k_work_delayable flash_work;
static uint8_t flashes_left;
static bool led_on;
static int last_profile = -1;

/* Turns the LED on and off in turn, rescheduling itself until the flashes run out. */
static void flash_work_cb(struct k_work *work) {
    if (led_on) {
        gpio_pin_set_dt(&led, 0);
        led_on = false;
        if (flashes_left > 0) {
            k_work_schedule(&flash_work, K_MSEC(CONFIG_BLE_PROFILE_LED_FLASH_MS));
        }
        return;
    }

    if (flashes_left > 0) {
        flashes_left--;
        gpio_pin_set_dt(&led, 1);
        led_on = true;
        k_work_schedule(&flash_work, K_MSEC(CONFIG_BLE_PROFILE_LED_FLASH_MS));
    }
}

static void start_flashes(uint8_t count) {
    k_work_cancel_delayable(&flash_work);
    gpio_pin_set_dt(&led, 0);
    led_on = false;
    flashes_left = count;

    if (count > 0) {
        k_work_schedule(&flash_work, K_NO_WAIT);
    }
}

/*
 * ZMK also raises this event when the active profile connects or disconnects, so
 * only flash when the profile index actually changes.
 */
static int ble_profile_led_listener(const zmk_event_t *eh) {
    const struct zmk_ble_active_profile_changed *ev = as_zmk_ble_active_profile_changed(eh);
    if (ev == NULL || ev->index == last_profile) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    last_profile = ev->index;
    LOG_DBG("active profile %d, flashing %d times", ev->index, ev->index + 1);
    start_flashes(ev->index + 1);

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(ble_profile_led, ble_profile_led_listener);
ZMK_SUBSCRIPTION(ble_profile_led, zmk_ble_active_profile_changed);

static int ble_profile_led_init(void) {
    if (!gpio_is_ready_dt(&led)) {
        LOG_ERR("LED GPIO is not ready");
        return -ENODEV;
    }

    int ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure LED GPIO: %d", ret);
        return ret;
    }

    k_work_init_delayable(&flash_work, flash_work_cb);

#if IS_ENABLED(CONFIG_BLE_PROFILE_LED_BOOT_TEST)
    start_flashes(3);
#endif

    return 0;
}

SYS_INIT(ble_profile_led_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
