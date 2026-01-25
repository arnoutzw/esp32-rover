/**
 * @file status_led.c
 * @brief Status LED indicator implementation
 *
 * REQ-39: Status LED Indicator
 * Uses GPIO 33 on ESP32-CAM (on-board red LED) to indicate WiFi status.
 * LED uses inverted logic: LOW = on, HIGH = off
 */

#include "status_led.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

// Include config for GPIO definition and target selection
#include "config.h"

static const char *TAG = "STATUS_LED";

// Only enable on ESP32-CAM (TTGO uses GPIO 33 for motor enable)
#if defined(ROVER_TARGET_ESP32CAM) && defined(STATUS_LED_GPIO)

// Blink periods in milliseconds
#ifndef STATUS_LED_STA_BLINK_PERIOD_MS
#define STATUS_LED_STA_BLINK_PERIOD_MS 1000  // 1Hz = 1000ms period
#endif

#ifndef STATUS_LED_AP_BLINK_PERIOD_MS
#define STATUS_LED_AP_BLINK_PERIOD_MS 500    // 2Hz = 500ms period
#endif

// Static state
static bool s_initialized = false;
static bool s_led_state = false;  // false = off, true = on
static status_led_mode_t s_current_mode = STATUS_LED_MODE_OFF;
static int64_t s_last_toggle_time = 0;

/**
 * @brief Set physical LED state (handles inverted logic)
 */
static void set_led_physical(bool on)
{
    // Inverted logic: LOW = LED on, HIGH = LED off
    gpio_set_level(STATUS_LED_GPIO, on ? 0 : 1);
    s_led_state = on;
}

esp_err_t status_led_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    // Configure GPIO as output
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << STATUS_LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO %d: %s", STATUS_LED_GPIO, esp_err_to_name(ret));
        return ret;
    }

    // Start with LED on (solid = powered)
    set_led_physical(true);
    s_current_mode = STATUS_LED_MODE_SOLID;
    s_last_toggle_time = esp_timer_get_time();
    s_initialized = true;

    ESP_LOGI(TAG, "Status LED initialized on GPIO %d", STATUS_LED_GPIO);
    return ESP_OK;
}

void status_led_update(bool is_sta_mode, bool is_connected)
{
    if (!s_initialized) {
        return;
    }

    // Determine desired mode based on WiFi state
    status_led_mode_t desired_mode;

    if (!is_sta_mode) {
        // AP mode - 2Hz blink
        desired_mode = STATUS_LED_MODE_BLINK_2HZ;
    } else if (is_connected) {
        // STA mode and connected - 1Hz blink
        desired_mode = STATUS_LED_MODE_BLINK_1HZ;
    } else {
        // STA mode but not connected - solid on
        desired_mode = STATUS_LED_MODE_SOLID;
    }

    // Update mode if changed
    if (desired_mode != s_current_mode) {
        s_current_mode = desired_mode;
        s_last_toggle_time = esp_timer_get_time();

        // Set initial state for new mode
        if (desired_mode == STATUS_LED_MODE_SOLID) {
            set_led_physical(true);
        } else if (desired_mode == STATUS_LED_MODE_OFF) {
            set_led_physical(false);
        }
    }

    // Handle blinking modes
    if (s_current_mode == STATUS_LED_MODE_BLINK_1HZ ||
        s_current_mode == STATUS_LED_MODE_BLINK_2HZ) {

        int64_t now = esp_timer_get_time();
        int64_t toggle_period_us;

        if (s_current_mode == STATUS_LED_MODE_BLINK_1HZ) {
            toggle_period_us = (STATUS_LED_STA_BLINK_PERIOD_MS * 1000) / 2;  // Half period for toggle
        } else {
            toggle_period_us = (STATUS_LED_AP_BLINK_PERIOD_MS * 1000) / 2;
        }

        if ((now - s_last_toggle_time) >= toggle_period_us) {
            set_led_physical(!s_led_state);
            s_last_toggle_time = now;
        }
    }
}

void status_led_set_mode(status_led_mode_t mode)
{
    if (!s_initialized) {
        return;
    }

    s_current_mode = mode;
    s_last_toggle_time = esp_timer_get_time();

    switch (mode) {
        case STATUS_LED_MODE_OFF:
            set_led_physical(false);
            break;
        case STATUS_LED_MODE_SOLID:
            set_led_physical(true);
            break;
        default:
            // Blink modes will be handled in update()
            break;
    }
}

status_led_mode_t status_led_get_mode(void)
{
    return s_current_mode;
}

void status_led_on(void)
{
    if (s_initialized) {
        set_led_physical(true);
    }
}

void status_led_off(void)
{
    if (s_initialized) {
        set_led_physical(false);
    }
}

void status_led_toggle(void)
{
    if (s_initialized) {
        set_led_physical(!s_led_state);
    }
}

#else
// Stub implementations for non-ESP32CAM targets

esp_err_t status_led_init(void)
{
    ESP_LOGW(TAG, "Status LED not available on this target");
    return ESP_ERR_NOT_SUPPORTED;
}

void status_led_update(bool is_sta_mode, bool is_connected)
{
    (void)is_sta_mode;
    (void)is_connected;
}

void status_led_set_mode(status_led_mode_t mode)
{
    (void)mode;
}

status_led_mode_t status_led_get_mode(void)
{
    return STATUS_LED_MODE_OFF;
}

void status_led_on(void) {}
void status_led_off(void) {}
void status_led_toggle(void) {}

#endif // ROVER_TARGET_ESP32CAM && STATUS_LED_GPIO
