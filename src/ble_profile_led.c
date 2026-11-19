/*
 * Flash the board's blue_led (profile index + 1) times when the active Bluetooth
 * profile changes: 1 flash for profile 0, 2 for profile 1, and so on. At boot (which
 * includes a sys reset and a wake from deep sleep) it first holds the LED on for one
 * long flash, so the keyboard turning on is not mistaken for a profile flash.
 *
 * The LED and the flash counters are only touched by flash_work_cb on the system
 * workqueue. The listener just records the requested count, so no lock is needed.
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>

LOG_MODULE_REGISTER(ble_profile_led, CONFIG_ZMK_LOG_LEVEL);

#define NO_REQUEST -1

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_NODELABEL(blue_led), gpios);

static struct k_work_delayable flash_work;

/* Shared with the listener, which can run on any thread. */
static atomic_t request = ATOMIC_INIT(NO_REQUEST);
static atomic_t last_profile = ATOMIC_INIT(-1);
static atomic_t led_ready = ATOMIC_INIT(0);

/* Owned by flash_work_cb. */
static bool led_on;
static uint8_t toggles_left;
static uint32_t on_ms;
static bool boot_flash_running;

static void set_led(bool on) {
    led_on = on;
    gpio_pin_set_dt(&led, on);
    LOG_DBG("led %s (toggles left %d, boot flash %d)", on ? "on" : "off", toggles_left,
            boot_flash_running);
}

/* One tick per LED toggle; each flash is an on toggle followed by an off toggle. */
static void flash_work_cb(struct k_work *work) {
    if (!boot_flash_running) {
        atomic_val_t count = atomic_set(&request, NO_REQUEST);

        if (count != NO_REQUEST) {
            /* Restart from off with a gap, so a flash cut short is not read as the next one. */
            LOG_DBG("starting %d profile flashes", (int)count);
            set_led(false);
            toggles_left = count * 2;
            on_ms = CONFIG_BLE_PROFILE_LED_FLASH_MS;
            k_work_schedule(&flash_work, K_MSEC(CONFIG_BLE_PROFILE_LED_FLASH_MS));
            return;
        }
    }

    if (toggles_left == 0) {
        if (boot_flash_running) {
            LOG_DBG("boot flash done");
        }
        boot_flash_running = false;
        if (atomic_get(&request) != NO_REQUEST) {
            k_work_schedule(&flash_work, K_NO_WAIT);
        }
        return;
    }

    set_led(!led_on);
    toggles_left--;

    /* After the last toggle, finish at once rather than waiting out one more gap, so a
     * profile flash queued behind the boot flash starts one gap later, not two. */
    k_timeout_t next =
        toggles_left == 0 ? K_NO_WAIT : K_MSEC(led_on ? on_ms : CONFIG_BLE_PROFILE_LED_FLASH_MS);
    k_work_schedule(&flash_work, next);
}

/*
 * ZMK also raises this event when the active profile connects or disconnects, so
 * only flash when the profile index actually changes.
 */
static int ble_profile_led_listener(const zmk_event_t *eh) {
    const struct zmk_ble_active_profile_changed *ev = as_zmk_ble_active_profile_changed(eh);
    if (ev == NULL || !atomic_get(&led_ready)) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (atomic_set(&last_profile, ev->index) == ev->index) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    LOG_DBG("active profile %d, flashing %d times", ev->index, ev->index + 1);
    atomic_set(&request, ev->index + 1);
    k_work_schedule(&flash_work, K_NO_WAIT);

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(ble_profile_led, ble_profile_led_listener);
ZMK_SUBSCRIPTION(ble_profile_led, zmk_ble_active_profile_changed);

static int ble_profile_led_init(void) {
    k_work_init_delayable(&flash_work, flash_work_cb);

    if (!gpio_is_ready_dt(&led)) {
        LOG_ERR("LED GPIO is not ready");
        return -ENODEV;
    }

    int ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure LED GPIO: %d", ret);
        return ret;
    }

    atomic_set(&led_ready, 1);

#if IS_ENABLED(CONFIG_BLE_PROFILE_LED_BOOT_FLASH)
    /* One long flash: on for BOOT_MS, then off. A profile flash waits for it to finish. */
    on_ms = CONFIG_BLE_PROFILE_LED_BOOT_MS;
    boot_flash_running = true;
    toggles_left = 2;
    k_work_schedule(&flash_work, K_NO_WAIT);
#endif

    return 0;
}

SYS_INIT(ble_profile_led_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
