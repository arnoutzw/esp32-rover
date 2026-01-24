#include "camera.h"
#include <string.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "CAMERA";

// Flash LED on ESP32-CAM is on GPIO 4
#define FLASH_LED_GPIO  GPIO_NUM_4

static bool camera_initialized = false;
static framesize_t current_frame_size = FRAMESIZE_VGA;

esp_err_t camera_module_init(const camera_config_params_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    if (camera_initialized) {
        ESP_LOGW(TAG, "Camera already initialized");
        return ESP_OK;
    }

    // Configure camera
    camera_config_t camera_config = {
        .pin_pwdn = config->pin_pwdn,
        .pin_reset = config->pin_reset,
        .pin_xclk = config->pin_xclk,
        .pin_sccb_sda = config->pin_sccb_sda,
        .pin_sccb_scl = config->pin_sccb_scl,
        .pin_d7 = config->pin_d7,
        .pin_d6 = config->pin_d6,
        .pin_d5 = config->pin_d5,
        .pin_d4 = config->pin_d4,
        .pin_d3 = config->pin_d3,
        .pin_d2 = config->pin_d2,
        .pin_d1 = config->pin_d1,
        .pin_d0 = config->pin_d0,
        .pin_vsync = config->pin_vsync,
        .pin_href = config->pin_href,
        .pin_pclk = config->pin_pclk,

        .xclk_freq_hz = config->xclk_freq,
        .ledc_timer = LEDC_TIMER_2,
        .ledc_channel = LEDC_CHANNEL_4,

        .pixel_format = config->pixel_format,
        .frame_size = config->frame_size,
        .jpeg_quality = config->jpeg_quality,
        .fb_count = config->fb_count,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
    };

    // Initialize camera
    esp_err_t ret = esp_camera_init(&camera_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", ret);
        return ret;
    }

    // Get sensor and apply default settings
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        // Default settings for better image quality
        s->set_brightness(s, 0);
        s->set_contrast(s, 0);
        s->set_saturation(s, 0);
        s->set_special_effect(s, 0);  // No effect
        s->set_whitebal(s, 1);        // Enable auto white balance
        s->set_awb_gain(s, 1);        // Enable AWB gain
        s->set_wb_mode(s, 0);         // Auto white balance mode
        s->set_exposure_ctrl(s, 1);   // Enable auto exposure
        s->set_aec2(s, 0);            // Disable AEC DSP
        s->set_gain_ctrl(s, 1);       // Enable auto gain
        s->set_agc_gain(s, 0);        // AGC gain
        s->set_gainceiling(s, (gainceiling_t)0);
        s->set_bpc(s, 0);             // Disable black pixel correction
        s->set_wpc(s, 1);             // Enable white pixel correction
        s->set_raw_gma(s, 1);         // Enable gamma correction
        s->set_lenc(s, 1);            // Enable lens correction
        s->set_hmirror(s, 0);         // No horizontal mirror
        s->set_vflip(s, 0);           // No vertical flip
        s->set_dcw(s, 1);             // Enable downsize
    }

    current_frame_size = config->frame_size;
    camera_initialized = true;

    ESP_LOGI(TAG, "Camera initialized successfully");
    return ESP_OK;
}

esp_err_t camera_module_deinit(void)
{
    if (!camera_initialized) {
        return ESP_OK;
    }

    esp_err_t ret = esp_camera_deinit();
    if (ret == ESP_OK) {
        camera_initialized = false;
    }
    return ret;
}

camera_fb_t* camera_capture_frame(void)
{
    if (!camera_initialized) {
        ESP_LOGE(TAG, "Camera not initialized");
        return NULL;
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "Failed to capture frame");
        return NULL;
    }

    return fb;
}

void camera_return_frame(camera_fb_t *fb)
{
    if (fb) {
        esp_camera_fb_return(fb);
    }
}

esp_err_t camera_set_resolution(framesize_t frame_size)
{
    if (!camera_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (!s) {
        return ESP_ERR_NOT_FOUND;
    }

    int ret = s->set_framesize(s, frame_size);
    if (ret == 0) {
        current_frame_size = frame_size;
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t camera_set_quality(int quality)
{
    if (!camera_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (quality < 0) quality = 0;
    if (quality > 63) quality = 63;

    sensor_t *s = esp_camera_sensor_get();
    if (!s) {
        return ESP_ERR_NOT_FOUND;
    }

    int ret = s->set_quality(s, quality);
    return ret == 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t camera_set_vflip(bool flip)
{
    if (!camera_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (!s) {
        return ESP_ERR_NOT_FOUND;
    }

    int ret = s->set_vflip(s, flip ? 1 : 0);
    return ret == 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t camera_set_hmirror(bool mirror)
{
    if (!camera_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (!s) {
        return ESP_ERR_NOT_FOUND;
    }

    int ret = s->set_hmirror(s, mirror ? 1 : 0);
    return ret == 0 ? ESP_OK : ESP_FAIL;
}

bool camera_is_initialized(void)
{
    return camera_initialized;
}

framesize_t camera_get_frame_size(void)
{
    return current_frame_size;
}

// =============================================================================
// Flash LED Control (ESP32-CAM GPIO 4)
// =============================================================================

static bool flash_led_initialized = false;
static bool flash_led_state = false;

esp_err_t flash_led_init(void)
{
    if (flash_led_initialized) {
        return ESP_OK;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << FLASH_LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure flash LED GPIO: 0x%x", ret);
        return ret;
    }

    // Start with LED off
    gpio_set_level(FLASH_LED_GPIO, 0);
    flash_led_state = false;
    flash_led_initialized = true;

    ESP_LOGI(TAG, "Flash LED initialized on GPIO %d", FLASH_LED_GPIO);
    return ESP_OK;
}

void flash_led_on(void)
{
    if (!flash_led_initialized) {
        flash_led_init();
    }
    gpio_set_level(FLASH_LED_GPIO, 1);
    flash_led_state = true;
}

void flash_led_off(void)
{
    if (!flash_led_initialized) {
        flash_led_init();
    }
    gpio_set_level(FLASH_LED_GPIO, 0);
    flash_led_state = false;
}

void flash_led_set(bool on)
{
    if (on) {
        flash_led_on();
    } else {
        flash_led_off();
    }
}

bool flash_led_get_state(void)
{
    return flash_led_state;
}

void flash_led_blink(int count, int on_ms, int off_ms)
{
    if (!flash_led_initialized) {
        flash_led_init();
    }

    bool previous_state = flash_led_state;

    for (int i = 0; i < count; i++) {
        gpio_set_level(FLASH_LED_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(on_ms));
        gpio_set_level(FLASH_LED_GPIO, 0);
        if (i < count - 1) {
            vTaskDelay(pdMS_TO_TICKS(off_ms));
        }
    }

    // Restore previous state
    gpio_set_level(FLASH_LED_GPIO, previous_state ? 1 : 0);
    flash_led_state = previous_state;
}
