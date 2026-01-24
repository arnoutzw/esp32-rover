/**
 * @file lcd_display.c
 * @brief LCD Display driver for TTGO T-Display (ST7789 135x240)
 */

#include "lcd_display.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>

static const char *TAG = "LCD_DISPLAY";

// ST7789 Display dimensions (TTGO T-Display is 135x240)
#define LCD_WIDTH   135
#define LCD_HEIGHT  240

// ST7789 Commands
#define ST7789_NOP       0x00
#define ST7789_SWRESET   0x01
#define ST7789_SLPIN     0x10
#define ST7789_SLPOUT    0x11
#define ST7789_NORON     0x13
#define ST7789_INVOFF    0x20
#define ST7789_INVON     0x21
#define ST7789_DISPOFF   0x28
#define ST7789_DISPON    0x29
#define ST7789_CASET     0x2A
#define ST7789_RASET     0x2B
#define ST7789_RAMWR     0x2C
#define ST7789_MADCTL    0x36
#define ST7789_COLMOD    0x3A

// Colors (RGB565)
#define COLOR_BLACK      0x0000
#define COLOR_WHITE      0xFFFF
#define COLOR_RED        0xF800
#define COLOR_GREEN      0x07E0
#define COLOR_BLUE       0x001F
#define COLOR_YELLOW     0xFFE0
#define COLOR_CYAN       0x07FF
#define COLOR_MAGENTA    0xF81F
#define COLOR_ORANGE     0xFD20
#define COLOR_DARKGRAY   0x4208
#define COLOR_LIGHTGRAY  0xC618

// Static variables
static spi_device_handle_t s_spi = NULL;
static int s_pin_dc = -1;
static int s_pin_backlight = -1;
static uint16_t s_framebuffer[LCD_WIDTH * 20]; // Partial framebuffer for text rows
static bool s_first_update = true; // Track first update to clear splash
static bool s_wifi_info_drawn = false; // Track if WiFi info has been drawn
static int8_t s_prev_button_left = -1; // Previous button states (-1 = not yet drawn)
static int8_t s_prev_button_right = -1;
static char s_prev_uptime_str[9] = ""; // Previous uptime string "HH:MM:SS" for digit-by-digit update

// Dirty tracking for all dynamic display elements (reduces SPI traffic, improves button latency)
static int8_t s_prev_connected = -1;     // Connection status (-1 = not yet drawn)
static int8_t s_prev_estop = -1;         // E-stop state
static int16_t s_prev_speed = INT16_MIN; // Speed percent
static int16_t s_prev_steer = INT16_MIN; // Steering degrees
static int16_t s_prev_velocity_x10 = INT16_MIN; // Velocity * 10 (for 0.1 precision)
static int16_t s_prev_battery_x100 = INT16_MIN; // Battery * 100 (for 0.01V precision)

// Basic 5x7 font (ASCII 32-127)
static const uint8_t font5x7[] = {
    0x00,0x00,0x00,0x00,0x00, // Space
    0x00,0x00,0x5F,0x00,0x00, // !
    0x00,0x07,0x00,0x07,0x00, // "
    0x14,0x7F,0x14,0x7F,0x14, // #
    0x24,0x2A,0x7F,0x2A,0x12, // $
    0x23,0x13,0x08,0x64,0x62, // %
    0x36,0x49,0x55,0x22,0x50, // &
    0x00,0x05,0x03,0x00,0x00, // '
    0x00,0x1C,0x22,0x41,0x00, // (
    0x00,0x41,0x22,0x1C,0x00, // )
    0x08,0x2A,0x1C,0x2A,0x08, // *
    0x08,0x08,0x3E,0x08,0x08, // +
    0x00,0x50,0x30,0x00,0x00, // ,
    0x08,0x08,0x08,0x08,0x08, // -
    0x00,0x60,0x60,0x00,0x00, // .
    0x20,0x10,0x08,0x04,0x02, // /
    0x3E,0x51,0x49,0x45,0x3E, // 0
    0x00,0x42,0x7F,0x40,0x00, // 1
    0x42,0x61,0x51,0x49,0x46, // 2
    0x21,0x41,0x45,0x4B,0x31, // 3
    0x18,0x14,0x12,0x7F,0x10, // 4
    0x27,0x45,0x45,0x45,0x39, // 5
    0x3C,0x4A,0x49,0x49,0x30, // 6
    0x01,0x71,0x09,0x05,0x03, // 7
    0x36,0x49,0x49,0x49,0x36, // 8
    0x06,0x49,0x49,0x29,0x1E, // 9
    0x00,0x36,0x36,0x00,0x00, // :
    0x00,0x56,0x36,0x00,0x00, // ;
    0x00,0x08,0x14,0x22,0x41, // <
    0x14,0x14,0x14,0x14,0x14, // =
    0x41,0x22,0x14,0x08,0x00, // >
    0x02,0x01,0x51,0x09,0x06, // ?
    0x32,0x49,0x79,0x41,0x3E, // @
    0x7E,0x11,0x11,0x11,0x7E, // A
    0x7F,0x49,0x49,0x49,0x36, // B
    0x3E,0x41,0x41,0x41,0x22, // C
    0x7F,0x41,0x41,0x22,0x1C, // D
    0x7F,0x49,0x49,0x49,0x41, // E
    0x7F,0x09,0x09,0x01,0x01, // F
    0x3E,0x41,0x41,0x51,0x32, // G
    0x7F,0x08,0x08,0x08,0x7F, // H
    0x00,0x41,0x7F,0x41,0x00, // I
    0x20,0x40,0x41,0x3F,0x01, // J
    0x7F,0x08,0x14,0x22,0x41, // K
    0x7F,0x40,0x40,0x40,0x40, // L
    0x7F,0x02,0x04,0x02,0x7F, // M
    0x7F,0x04,0x08,0x10,0x7F, // N
    0x3E,0x41,0x41,0x41,0x3E, // O
    0x7F,0x09,0x09,0x09,0x06, // P
    0x3E,0x41,0x51,0x21,0x5E, // Q
    0x7F,0x09,0x19,0x29,0x46, // R
    0x46,0x49,0x49,0x49,0x31, // S
    0x01,0x01,0x7F,0x01,0x01, // T
    0x3F,0x40,0x40,0x40,0x3F, // U
    0x1F,0x20,0x40,0x20,0x1F, // V
    0x7F,0x20,0x18,0x20,0x7F, // W
    0x63,0x14,0x08,0x14,0x63, // X
    0x03,0x04,0x78,0x04,0x03, // Y
    0x61,0x51,0x49,0x45,0x43, // Z
    0x00,0x00,0x7F,0x41,0x41, // [
    0x02,0x04,0x08,0x10,0x20, // backslash
    0x41,0x41,0x7F,0x00,0x00, // ]
    0x04,0x02,0x01,0x02,0x04, // ^
    0x40,0x40,0x40,0x40,0x40, // _
    0x00,0x01,0x02,0x04,0x00, // `
    0x20,0x54,0x54,0x54,0x78, // a
    0x7F,0x48,0x44,0x44,0x38, // b
    0x38,0x44,0x44,0x44,0x20, // c
    0x38,0x44,0x44,0x48,0x7F, // d
    0x38,0x54,0x54,0x54,0x18, // e
    0x08,0x7E,0x09,0x01,0x02, // f
    0x08,0x14,0x54,0x54,0x3C, // g
    0x7F,0x08,0x04,0x04,0x78, // h
    0x00,0x44,0x7D,0x40,0x00, // i
    0x20,0x40,0x44,0x3D,0x00, // j
    0x00,0x7F,0x10,0x28,0x44, // k
    0x00,0x41,0x7F,0x40,0x00, // l
    0x7C,0x04,0x18,0x04,0x78, // m
    0x7C,0x08,0x04,0x04,0x78, // n
    0x38,0x44,0x44,0x44,0x38, // o
    0x7C,0x14,0x14,0x14,0x08, // p
    0x08,0x14,0x14,0x18,0x7C, // q
    0x7C,0x08,0x04,0x04,0x08, // r
    0x48,0x54,0x54,0x54,0x20, // s
    0x04,0x3F,0x44,0x40,0x20, // t
    0x3C,0x40,0x40,0x20,0x7C, // u
    0x1C,0x20,0x40,0x20,0x1C, // v
    0x3C,0x40,0x30,0x40,0x3C, // w
    0x44,0x28,0x10,0x28,0x44, // x
    0x0C,0x50,0x50,0x50,0x3C, // y
    0x44,0x64,0x54,0x4C,0x44, // z
    0x00,0x08,0x36,0x41,0x00, // {
    0x00,0x00,0x7F,0x00,0x00, // |
    0x00,0x41,0x36,0x08,0x00, // }
    0x08,0x08,0x2A,0x1C,0x08, // ~
    0x08,0x1C,0x2A,0x08,0x08, // DEL (arrow)
};

// Send command to LCD
static void lcd_cmd(uint8_t cmd)
{
    gpio_set_level(s_pin_dc, 0);
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    spi_device_transmit(s_spi, &t);
}

// Send data to LCD
static void lcd_data(const uint8_t *data, size_t len)
{
    if (len == 0) return;
    gpio_set_level(s_pin_dc, 1);
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    spi_device_transmit(s_spi, &t);
}

// Send single byte of data
static void lcd_data_byte(uint8_t data)
{
    lcd_data(&data, 1);
}

// Set drawing window
static void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    // Column offset for TTGO T-Display
    uint16_t col_offset = 52;
    uint16_t row_offset = 40;

    lcd_cmd(ST7789_CASET);
    uint8_t col_data[] = {
        (x0 + col_offset) >> 8, (x0 + col_offset) & 0xFF,
        (x1 + col_offset) >> 8, (x1 + col_offset) & 0xFF
    };
    lcd_data(col_data, 4);

    lcd_cmd(ST7789_RASET);
    uint8_t row_data[] = {
        (y0 + row_offset) >> 8, (y0 + row_offset) & 0xFF,
        (y1 + row_offset) >> 8, (y1 + row_offset) & 0xFF
    };
    lcd_data(row_data, 4);

    lcd_cmd(ST7789_RAMWR);
}

// Fill rectangle with color
static void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    lcd_set_window(x, y, x + w - 1, y + h - 1);

    // Swap bytes for SPI (big endian)
    uint16_t color_be = (color >> 8) | (color << 8);

    gpio_set_level(s_pin_dc, 1);

    // Send in chunks
    uint16_t chunk[64];
    for (int i = 0; i < 64; i++) {
        chunk[i] = color_be;
    }

    size_t total = w * h;
    while (total > 0) {
        size_t send = (total > 64) ? 64 : total;
        spi_transaction_t t = {
            .length = send * 16,
            .tx_buffer = chunk,
        };
        spi_device_transmit(s_spi, &t);
        total -= send;
    }
}

// Draw character at position
static void lcd_draw_char(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale)
{
    if (c < 32 || c > 127) c = '?';
    const uint8_t *glyph = &font5x7[(c - 32) * 5];

    uint16_t fg_be = (fg >> 8) | (fg << 8);
    uint16_t bg_be = (bg >> 8) | (bg << 8);

    for (int col = 0; col < 5; col++) {
        uint8_t line = glyph[col];
        for (int row = 0; row < 7; row++) {
            uint16_t color = (line & (1 << row)) ? fg_be : bg_be;
            if (scale == 1) {
                lcd_set_window(x + col, y + row, x + col, y + row);
                lcd_data((uint8_t*)&color, 2);
            } else {
                lcd_fill_rect(x + col * scale, y + row * scale, scale, scale,
                             (line & (1 << row)) ? fg : bg);
            }
        }
    }
    // Space between characters
    if (scale == 1) {
        for (int row = 0; row < 7; row++) {
            lcd_set_window(x + 5, y + row, x + 5, y + row);
            lcd_data((uint8_t*)&bg_be, 2);
        }
    } else {
        lcd_fill_rect(x + 5 * scale, y, scale, 7 * scale, bg);
    }
}

// Draw string at position
static void lcd_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg, uint8_t scale)
{
    while (*str) {
        lcd_draw_char(x, y, *str, fg, bg, scale);
        x += 6 * scale;
        str++;
    }
}

// Draw horizontal progress bar
static void lcd_draw_bar(uint16_t x, uint16_t y, uint16_t w, uint16_t h, int value, int min_val, int max_val, uint16_t fg, uint16_t bg)
{
    // Background
    lcd_fill_rect(x, y, w, h, bg);

    // Calculate fill
    int range = max_val - min_val;
    if (range == 0) return;

    int center = w / 2;
    int val_normalized = value - min_val;
    int fill_width = (val_normalized * w) / range;

    if (min_val < 0 && max_val > 0) {
        // Bi-directional bar (e.g., speed -100 to +100)
        if (value >= 0) {
            int bar_w = (value * (w / 2)) / max_val;
            lcd_fill_rect(x + center, y + 1, bar_w, h - 2, fg);
        } else {
            int bar_w = (-value * (w / 2)) / (-min_val);
            lcd_fill_rect(x + center - bar_w, y + 1, bar_w, h - 2, fg);
        }
        // Center line
        lcd_fill_rect(x + center - 1, y, 2, h, COLOR_WHITE);
    } else {
        // Standard bar
        lcd_fill_rect(x + 1, y + 1, fill_width - 2, h - 2, fg);
    }
}

esp_err_t lcd_display_init(const lcd_display_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    s_pin_dc = config->pin_dc;
    s_pin_backlight = config->pin_backlight;

    // Configure DC pin
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << config->pin_dc) | (1ULL << config->pin_rst),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Configure backlight with PWM
    if (config->pin_backlight >= 0) {
        ledc_timer_config_t ledc_timer = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .duty_resolution = LEDC_TIMER_8_BIT,
            .timer_num = LEDC_TIMER_1,
            .freq_hz = 5000,
            .clk_cfg = LEDC_AUTO_CLK,
        };
        ledc_timer_config(&ledc_timer);

        ledc_channel_config_t ledc_channel = {
            .gpio_num = config->pin_backlight,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_5,  // Use channel 5 to avoid conflict with BLDC motor (0,1,2)
            .timer_sel = LEDC_TIMER_1,
            .duty = 255,
            .hpoint = 0,
        };
        ledc_channel_config(&ledc_channel);
    }

    // Initialize SPI
    spi_bus_config_t buscfg = {
        .mosi_io_num = config->pin_mosi,
        .miso_io_num = -1,
        .sclk_io_num = config->pin_sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * LCD_HEIGHT * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 40 * 1000 * 1000,  // 40 MHz (ST7789 supports up to 80MHz)
        .mode = 0,
        .spics_io_num = config->pin_cs,
        .queue_size = 7,
        .flags = SPI_DEVICE_NO_DUMMY | SPI_DEVICE_HALFDUPLEX,  // Write-only display, enables higher clock speeds
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &s_spi));

    // Hardware reset
    gpio_set_level(config->pin_rst, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(config->pin_rst, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Initialize display
    lcd_cmd(ST7789_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(150));

    lcd_cmd(ST7789_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(120));

    lcd_cmd(ST7789_COLMOD);
    lcd_data_byte(0x55);  // 16-bit color

    lcd_cmd(ST7789_MADCTL);
    lcd_data_byte(0x00);  // RGB order, no rotation

    lcd_cmd(ST7789_INVON);  // Inversion on for TTGO T-Display

    lcd_cmd(ST7789_NORON);
    vTaskDelay(pdMS_TO_TICKS(10));

    lcd_cmd(ST7789_DISPON);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Clear screen
    lcd_display_clear();

    ESP_LOGI(TAG, "LCD display initialized (ST7789 %dx%d)", LCD_WIDTH, LCD_HEIGHT);

    return ESP_OK;
}

esp_err_t lcd_display_clear(void)
{
    lcd_fill_rect(0, 0, LCD_WIDTH, LCD_HEIGHT, COLOR_BLACK);
    return ESP_OK;
}

esp_err_t lcd_display_splash(void)
{
    lcd_display_clear();

    // Title
    lcd_draw_string(20, 40, "ESP32", COLOR_CYAN, COLOR_BLACK, 2);
    lcd_draw_string(20, 70, "ROVER", COLOR_GREEN, COLOR_BLACK, 2);

    // Version
    lcd_draw_string(20, 110, "v1.0", COLOR_WHITE, COLOR_BLACK, 1);

    // Status
    lcd_draw_string(20, 140, "Starting...", COLOR_YELLOW, COLOR_BLACK, 1);

    return ESP_OK;
}

esp_err_t lcd_display_update(const lcd_rover_status_t *status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }

    // Clear screen on first update to remove splash
    if (s_first_update) {
        lcd_display_clear();
        s_first_update = false;
    }

    char buf[32];
    int y;

    // =========================================================================
    // BUTTON INDICATORS - Draw FIRST for lowest latency!
    // =========================================================================
    y = 132;  // Fixed position for buttons (calculated from layout)
    int btn_width = LCD_WIDTH / 2;
    int btn_height = 20;

    // Left button - only redraw if state changed (-1 means first draw)
    if ((int8_t)status->button_left != s_prev_button_left) {
        uint16_t btn_l_bg = status->button_left ? COLOR_CYAN : COLOR_DARKGRAY;
        uint16_t btn_l_fg = status->button_left ? COLOR_BLACK : COLOR_LIGHTGRAY;
        lcd_fill_rect(0, y, btn_width - 1, btn_height, btn_l_bg);
        lcd_draw_string(btn_width / 2 - 6, y + 6, "L", btn_l_fg, btn_l_bg, 1);
        s_prev_button_left = (int8_t)status->button_left;
    }

    // Right button - only redraw if state changed (-1 means first draw)
    if ((int8_t)status->button_right != s_prev_button_right) {
        uint16_t btn_r_bg = status->button_right ? COLOR_CYAN : COLOR_DARKGRAY;
        uint16_t btn_r_fg = status->button_right ? COLOR_BLACK : COLOR_LIGHTGRAY;
        lcd_fill_rect(btn_width + 1, y, btn_width - 1, btn_height, btn_r_bg);
        lcd_draw_string(btn_width + btn_width / 2 - 6, y + 6, "R", btn_r_fg, btn_r_bg, 1);
        s_prev_button_right = (int8_t)status->button_right;
    }

    // =========================================================================
    // HEADER BAR - Only redraw on connection state change
    // =========================================================================
    if ((int8_t)status->connected != s_prev_connected) {
        uint16_t header_color = status->connected ? COLOR_GREEN : COLOR_RED;
        lcd_fill_rect(0, 0, LCD_WIDTH, 16, header_color);
        lcd_draw_string(4, 4, status->connected ? "OK" : "..", COLOR_WHITE, header_color, 1);
        s_prev_connected = (int8_t)status->connected;
    }

    // =========================================================================
    // E-STOP INDICATOR - Only redraw on state change
    // =========================================================================
    if ((int8_t)status->estop != s_prev_estop) {
        if (status->estop) {
            lcd_fill_rect(0, 18, LCD_WIDTH, 14, COLOR_RED);
            lcd_draw_string(25, 21, "!! E-STOP !!", COLOR_WHITE, COLOR_RED, 1);
        } else {
            lcd_fill_rect(0, 18, LCD_WIDTH, 14, COLOR_BLACK);
        }
        s_prev_estop = (int8_t)status->estop;
    }

    y = 34;

    // =========================================================================
    // SPEED SECTION - Only redraw on value change
    // =========================================================================
    if (status->speed_percent != s_prev_speed) {
        // Label (draw once on first change from INT16_MIN)
        if (s_prev_speed == INT16_MIN) {
            lcd_draw_string(4, y, "SPEED", COLOR_LIGHTGRAY, COLOR_BLACK, 1);
        }
        // Value
        snprintf(buf, sizeof(buf), "%+4d%%", status->speed_percent);
        uint16_t speed_color = (status->speed_percent == 0) ? COLOR_WHITE :
                              (status->speed_percent > 0) ? COLOR_GREEN : COLOR_ORANGE;
        lcd_fill_rect(70, y, 60, 10, COLOR_BLACK);  // Clear old value
        lcd_draw_string(70, y, buf, speed_color, COLOR_BLACK, 1);

        // Speed bar
        lcd_draw_bar(4, y + 12, LCD_WIDTH - 8, 12, status->speed_percent, -100, 100, COLOR_GREEN, COLOR_DARKGRAY);

        s_prev_speed = status->speed_percent;
    }
    y += 32;

    // =========================================================================
    // STEERING SECTION - Only redraw on value change
    // =========================================================================
    if (status->steering_degrees != s_prev_steer) {
        // Label (draw once on first change)
        if (s_prev_steer == INT16_MIN) {
            lcd_draw_string(4, y, "STEER", COLOR_LIGHTGRAY, COLOR_BLACK, 1);
            lcd_draw_char(106, y, 0x7E, COLOR_CYAN, COLOR_BLACK, 1); // degree symbol
        }
        // Value
        snprintf(buf, sizeof(buf), "%+4d", status->steering_degrees);
        lcd_fill_rect(70, y, 35, 10, COLOR_BLACK);  // Clear old value
        lcd_draw_string(70, y, buf, COLOR_CYAN, COLOR_BLACK, 1);

        // Steering bar
        lcd_draw_bar(4, y + 12, LCD_WIDTH - 8, 12, status->steering_degrees, -45, 45, COLOR_CYAN, COLOR_DARKGRAY);

        s_prev_steer = status->steering_degrees;
    }
    y += 32;

    // =========================================================================
    // VELOCITY SECTION - Only redraw on value change (0.1 precision)
    // =========================================================================
    int16_t vel_x10 = (int16_t)(status->velocity_rads * 10);
    if (vel_x10 != s_prev_velocity_x10) {
        // Label (draw once)
        if (s_prev_velocity_x10 == INT16_MIN) {
            lcd_draw_string(4, y, "VEL", COLOR_LIGHTGRAY, COLOR_BLACK, 1);
        }
        snprintf(buf, sizeof(buf), "%5.1f r/s", status->velocity_rads);
        lcd_fill_rect(50, y, 80, 10, COLOR_BLACK);  // Clear old value
        lcd_draw_string(50, y, buf, COLOR_YELLOW, COLOR_BLACK, 1);
        s_prev_velocity_x10 = vel_x10;
    }
    y += 16;

    // =========================================================================
    // BATTERY SECTION - Only redraw on value change (0.01V precision)
    // =========================================================================
    int16_t bat_x100 = (int16_t)(status->battery_volts * 100);
    if (bat_x100 != s_prev_battery_x100) {
        // Label (draw once)
        if (s_prev_battery_x100 == INT16_MIN) {
            lcd_draw_string(4, y, "BAT", COLOR_LIGHTGRAY, COLOR_BLACK, 1);
        }
        snprintf(buf, sizeof(buf), "%4.2fV", status->battery_volts);
        uint16_t bat_color = (status->battery_volts > 3.7f) ? COLOR_GREEN :
                            (status->battery_volts > 3.4f) ? COLOR_YELLOW : COLOR_RED;
        lcd_fill_rect(50, y, 45, 10, COLOR_BLACK);  // Clear old value
        lcd_draw_string(50, y, buf, bat_color, COLOR_BLACK, 1);

        // Battery bar (3.0V = 0%, 4.2V = 100%)
        int bat_pct = (int)((status->battery_volts - 3.0f) / (4.2f - 3.0f) * 100);
        if (bat_pct < 0) bat_pct = 0;
        if (bat_pct > 100) bat_pct = 100;
        lcd_fill_rect(95, y, 36, 10, COLOR_DARKGRAY);
        lcd_fill_rect(96, y + 1, (bat_pct * 34) / 100, 8, bat_color);

        s_prev_battery_x100 = bat_x100;
    }

    // =========================================================================
    // INFO SECTION (WiFi/MAC drawn once, uptime updated per-character)
    // =========================================================================
    int info_y = LCD_HEIGHT - 42;  // Taller section for more info

    if (!s_wifi_info_drawn) {
        lcd_fill_rect(0, info_y, LCD_WIDTH, 42, COLOR_DARKGRAY);
        // SSID and IP on first line
        if (status->wifi_ssid) {
            lcd_draw_string(4, info_y + 2, status->wifi_ssid, COLOR_WHITE, COLOR_DARKGRAY, 1);
        }
        if (status->wifi_ip) {
            lcd_draw_string(4, info_y + 12, status->wifi_ip, COLOR_CYAN, COLOR_DARKGRAY, 1);
        }
        // MAC address on second line
        if (status->mac_addr) {
            lcd_draw_string(4, info_y + 22, status->mac_addr, COLOR_YELLOW, COLOR_DARKGRAY, 1);
        }
        s_wifi_info_drawn = true;
    }

    // Uptime (only redraw digits that changed)
    {
        uint32_t secs = status->uptime_secs;
        uint32_t mins = secs / 60;
        uint32_t hrs = mins / 60;
        secs %= 60;
        mins %= 60;
        if (hrs > 99) hrs = 99;  // Cap at 99 hours for display

        // Format as HH:MM:SS (always exactly 8 characters)
        char uptime_str[9];
        uptime_str[0] = '0' + (hrs / 10);
        uptime_str[1] = '0' + (hrs % 10);
        uptime_str[2] = ':';
        uptime_str[3] = '0' + (mins / 10);
        uptime_str[4] = '0' + (mins % 10);
        uptime_str[5] = ':';
        uptime_str[6] = '0' + (secs / 10);
        uptime_str[7] = '0' + (secs % 10);
        uptime_str[8] = '\0';

        // Compare character by character and only redraw changed digits
        int uptime_x = 4;
        int uptime_y = info_y + 32;
        int char_width = 6; // 5 pixel font + 1 pixel spacing

        for (int i = 0; i < 8; i++) {
            if (s_prev_uptime_str[i] != uptime_str[i]) {
                // This character changed, redraw it
                lcd_draw_char(uptime_x + i * char_width, uptime_y, uptime_str[i],
                             COLOR_GREEN, COLOR_DARKGRAY, 1);
            }
        }

        // Update previous string
        memcpy(s_prev_uptime_str, uptime_str, 9);
    }

    return ESP_OK;
}

esp_err_t lcd_display_set_backlight(uint8_t brightness)
{
    if (s_pin_backlight >= 0) {
        uint32_t duty = (brightness * 255) / 100;
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_5, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_5);
    }
    return ESP_OK;
}

esp_err_t lcd_display_diagnostics(const lcd_wifi_diag_t *diag)
{
    if (!diag) {
        return ESP_ERR_INVALID_ARG;
    }

    char buf[32];
    int y = 0;

    // Header
    lcd_fill_rect(0, y, LCD_WIDTH, 16, COLOR_MAGENTA);
    lcd_draw_string(20, y + 4, "DIAGNOSTICS", COLOR_WHITE, COLOR_MAGENTA, 1);
    y += 18;

    // WiFi Section
    lcd_fill_rect(0, y, LCD_WIDTH, 10, COLOR_DARKGRAY);
    lcd_draw_string(4, y + 2, "-- WiFi --", COLOR_CYAN, COLOR_DARKGRAY, 1);
    y += 12;

    // SSID
    lcd_draw_string(4, y, "SSID:", COLOR_LIGHTGRAY, COLOR_BLACK, 1);
    if (diag->ssid) {
        lcd_draw_string(40, y, diag->ssid, COLOR_WHITE, COLOR_BLACK, 1);
    }
    y += 10;

    // Channel
    lcd_draw_string(4, y, "Chan:", COLOR_LIGHTGRAY, COLOR_BLACK, 1);
    snprintf(buf, sizeof(buf), "%d", diag->channel);
    lcd_draw_string(40, y, buf, COLOR_GREEN, COLOR_BLACK, 1);

    // TX Power
    lcd_draw_string(70, y, "TX:", COLOR_LIGHTGRAY, COLOR_BLACK, 1);
    snprintf(buf, sizeof(buf), "%ddBm", diag->tx_power);
    lcd_draw_string(94, y, buf, COLOR_GREEN, COLOR_BLACK, 1);
    y += 10;

    // Connected stations
    lcd_draw_string(4, y, "Clients:", COLOR_LIGHTGRAY, COLOR_BLACK, 1);
    snprintf(buf, sizeof(buf), "%d", diag->connected_stations);
    lcd_draw_string(58, y, buf, COLOR_YELLOW, COLOR_BLACK, 1);
    y += 10;

    // IP Address
    lcd_draw_string(4, y, "IP:", COLOR_LIGHTGRAY, COLOR_BLACK, 1);
    if (diag->ip_addr) {
        lcd_draw_string(28, y, diag->ip_addr, COLOR_CYAN, COLOR_BLACK, 1);
    }
    y += 10;

    // MAC Address
    lcd_draw_string(4, y, "MAC:", COLOR_LIGHTGRAY, COLOR_BLACK, 1);
    if (diag->mac_addr) {
        lcd_draw_string(34, y, diag->mac_addr, COLOR_YELLOW, COLOR_BLACK, 1);
    }
    y += 12;

    // Memory Section
    lcd_fill_rect(0, y, LCD_WIDTH, 10, COLOR_DARKGRAY);
    lcd_draw_string(4, y + 2, "-- Memory --", COLOR_CYAN, COLOR_DARKGRAY, 1);
    y += 12;

    // RAM usage with percentage bar
    uint32_t used_heap = diag->total_heap - diag->free_heap;
    uint8_t heap_pct = (diag->total_heap > 0) ? (used_heap * 100 / diag->total_heap) : 0;

    lcd_draw_string(4, y, "RAM:", COLOR_LIGHTGRAY, COLOR_BLACK, 1);
    snprintf(buf, sizeof(buf), "%lu/%luK", (unsigned long)(used_heap / 1024),
             (unsigned long)(diag->total_heap / 1024));
    lcd_draw_string(34, y, buf, COLOR_WHITE, COLOR_BLACK, 1);
    y += 10;

    // RAM usage bar
    uint16_t bar_color = (heap_pct < 70) ? COLOR_GREEN : (heap_pct < 90) ? COLOR_YELLOW : COLOR_RED;
    lcd_fill_rect(4, y, LCD_WIDTH - 8, 6, COLOR_DARKGRAY);
    lcd_fill_rect(4, y, (heap_pct * (LCD_WIDTH - 8)) / 100, 6, bar_color);
    snprintf(buf, sizeof(buf), "%d%%", heap_pct);
    lcd_draw_string(LCD_WIDTH - 24, y, buf, COLOR_WHITE, COLOR_BLACK, 1);
    y += 8;

    // Internal RAM
    lcd_draw_string(4, y, "Int:", COLOR_LIGHTGRAY, COLOR_BLACK, 1);
    snprintf(buf, sizeof(buf), "%luK", (unsigned long)(diag->free_internal / 1024));
    lcd_draw_string(34, y, buf, COLOR_GREEN, COLOR_BLACK, 1);

    // Min heap (watermark)
    lcd_draw_string(70, y, "Lo:", COLOR_LIGHTGRAY, COLOR_BLACK, 1);
    snprintf(buf, sizeof(buf), "%luK", (unsigned long)(diag->min_free_heap / 1024));
    lcd_draw_string(94, y, buf, COLOR_YELLOW, COLOR_BLACK, 1);
    y += 12;

    // System Section
    lcd_fill_rect(0, y, LCD_WIDTH, 10, COLOR_DARKGRAY);
    lcd_draw_string(4, y + 2, "-- System --", COLOR_CYAN, COLOR_DARKGRAY, 1);
    y += 12;

    // CPU frequency
    lcd_draw_string(4, y, "CPU:", COLOR_LIGHTGRAY, COLOR_BLACK, 1);
    snprintf(buf, sizeof(buf), "%.0fMHz", diag->cpu_freq_mhz);
    lcd_draw_string(34, y, buf, COLOR_GREEN, COLOR_BLACK, 1);
    y += 10;

    // Tasks per core (REQ-09)
    lcd_draw_string(4, y, "Tasks:", COLOR_LIGHTGRAY, COLOR_BLACK, 1);
    snprintf(buf, sizeof(buf), "C0:%d C1:%d", diag->tasks_core0, diag->tasks_core1);
    lcd_draw_string(46, y, buf, COLOR_CYAN, COLOR_BLACK, 1);
    y += 10;

    // Battery and Uptime on same line
    lcd_draw_string(4, y, "Bat:", COLOR_LIGHTGRAY, COLOR_BLACK, 1);
    snprintf(buf, sizeof(buf), "%.2fV", diag->battery_volts);
    uint16_t bat_color = (diag->battery_volts > 3.7f) ? COLOR_GREEN :
                        (diag->battery_volts > 3.4f) ? COLOR_YELLOW : COLOR_RED;
    lcd_draw_string(34, y, buf, bat_color, COLOR_BLACK, 1);

    // Uptime
    uint32_t secs = diag->uptime_secs;
    uint32_t mins = secs / 60;
    uint32_t hrs = mins / 60;
    secs %= 60;
    mins %= 60;
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", (unsigned long)hrs, (unsigned long)mins, (unsigned long)secs);
    lcd_draw_string(85, y, buf, COLOR_GREEN, COLOR_BLACK, 1);
    y += 12;

    // Services Section
    lcd_fill_rect(0, y, LCD_WIDTH, 10, COLOR_DARKGRAY);
    lcd_draw_string(4, y + 2, "-- Services --", COLOR_CYAN, COLOR_DARKGRAY, 1);
    y += 12;

    // REST API status
    lcd_draw_string(4, y, "REST:", COLOR_LIGHTGRAY, COLOR_BLACK, 1);
    lcd_draw_string(40, y, diag->rest_api_enabled ? "ON" : "OFF",
                   diag->rest_api_enabled ? COLOR_GREEN : COLOR_RED, COLOR_BLACK, 1);

    // MQTT status
    lcd_draw_string(70, y, "MQTT:", COLOR_LIGHTGRAY, COLOR_BLACK, 1);
    if (!diag->mqtt_enabled) {
        lcd_draw_string(106, y, "OFF", COLOR_RED, COLOR_BLACK, 1);
    } else if (diag->mqtt_connected) {
        lcd_draw_string(106, y, "OK", COLOR_GREEN, COLOR_BLACK, 1);
    } else {
        lcd_draw_string(106, y, "...", COLOR_YELLOW, COLOR_BLACK, 1);
    }
    y += 10;

    // Internet connectivity (REQ-10)
    lcd_draw_string(4, y, "Internet:", COLOR_LIGHTGRAY, COLOR_BLACK, 1);
    lcd_draw_string(64, y, diag->internet_connected ? "Connected" : "Offline",
                   diag->internet_connected ? COLOR_GREEN : COLOR_RED, COLOR_BLACK, 1);
    y += 12;

    // Local time (REQ-13)
    lcd_draw_string(4, y, "Time:", COLOR_LIGHTGRAY, COLOR_BLACK, 1);
    if (diag->local_time && diag->ntp_synced) {
        lcd_draw_string(40, y, diag->local_time, COLOR_GREEN, COLOR_BLACK, 1);
    } else {
        lcd_draw_string(40, y, "--:--:--", COLOR_YELLOW, COLOR_BLACK, 1);
    }
    y += 12;

    // Footer with exit instruction
    lcd_fill_rect(0, LCD_HEIGHT - 14, LCD_WIDTH, 14, COLOR_DARKGRAY);
    lcd_draw_string(8, LCD_HEIGHT - 10, "Press any btn to exit", COLOR_WHITE, COLOR_DARKGRAY, 1);

    return ESP_OK;
}

void lcd_display_reset_state(void)
{
    // Reset all tracking state so next update redraws everything
    s_first_update = true;
    s_wifi_info_drawn = false;
    s_prev_button_left = -1;
    s_prev_button_right = -1;
    memset(s_prev_uptime_str, 0, sizeof(s_prev_uptime_str));

    // Reset new dirty tracking state
    s_prev_connected = -1;
    s_prev_estop = -1;
    s_prev_speed = INT16_MIN;
    s_prev_steer = INT16_MIN;
    s_prev_velocity_x10 = INT16_MIN;
    s_prev_battery_x100 = INT16_MIN;
}

// =============================================================================
// Sleep Screen with Snorlax Sprite (REQ-30)
// =============================================================================

// Snorlax sprite colors (RGB565)
#define SNORLAX_BODY     0x2146  // Dark teal/blue body
#define SNORLAX_BELLY    0xFED6  // Cream/beige belly
#define SNORLAX_FACE     0xFED6  // Same as belly for face
#define SNORLAX_OUTLINE  0x0000  // Black outline
#define SNORLAX_FEET     0xFED6  // Cream feet/claws

// 32x32 Snorlax sleeping sprite - each byte is a color index
// 0=transparent, 1=outline, 2=body, 3=belly/face, 4=closed eyes
static const uint8_t snorlax_sprite[32][32] = {
    //0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    { 0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0 }, // 0
    { 0,0,0,0,0,0,0,0,0,1,1,2,2,2,2,2,2,2,2,2,2,1,1,0,0,0,0,0,0,0,0,0 }, // 1
    { 0,0,0,0,0,0,0,0,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0,0,0,0,0,0,0,0 }, // 2
    { 0,0,0,0,0,0,0,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0,0,0,0,0,0,0 }, // 3
    { 0,0,0,0,0,0,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0,0,0,0,0,0 }, // 4
    { 0,0,0,0,0,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0,0,0,0,0 }, // 5
    { 0,0,0,0,1,2,2,2,2,2,1,1,1,2,2,2,2,2,1,1,1,2,2,2,2,2,2,1,0,0,0,0 }, // 6  ears
    { 0,0,0,1,2,2,2,2,2,1,3,3,3,1,2,2,2,1,3,3,3,1,2,2,2,2,2,2,1,0,0,0 }, // 7  ears
    { 0,0,0,1,2,2,2,2,2,1,3,3,3,1,2,2,2,1,3,3,3,1,2,2,2,2,2,2,1,0,0,0 }, // 8
    { 0,0,1,2,2,2,2,2,2,2,1,1,1,2,2,2,2,2,1,1,1,2,2,2,2,2,2,2,2,1,0,0 }, // 9
    { 0,0,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0,0 }, // 10
    { 0,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0 }, // 11
    { 0,1,2,2,2,2,2,1,1,1,1,1,2,2,2,2,2,2,1,1,1,1,1,2,2,2,2,2,2,2,1,0 }, // 12 closed eyes
    { 0,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0 }, // 13
    { 0,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0 }, // 14
    { 1,2,2,2,2,2,2,2,2,2,2,2,2,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,1 }, // 15 mouth
    { 1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1 }, // 16
    { 1,2,2,2,2,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,1 }, // 17 belly top
    { 1,2,2,2,2,2,2,1,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,1,2,2,2,2,2,2,1 }, // 18
    { 1,2,2,2,2,2,1,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,1,2,2,2,2,2,1 }, // 19
    { 1,2,2,2,2,2,1,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,1,2,2,2,2,2,1 }, // 20
    { 1,2,2,2,2,2,1,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,1,2,2,2,2,2,1 }, // 21
    { 1,2,2,2,2,2,1,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,1,2,2,2,2,2,1 }, // 22
    { 1,2,2,2,2,2,2,1,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,1,2,2,2,2,2,2,1 }, // 23
    { 0,1,2,2,2,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,1,0 }, // 24 belly bottom
    { 0,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0 }, // 25
    { 0,0,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0,0 }, // 26
    { 0,0,1,2,2,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,1,1,2,2,2,1,0,0 }, // 27 feet
    { 0,0,0,1,1,3,3,3,1,1,2,2,2,2,2,2,2,2,2,2,2,2,1,3,3,3,1,1,1,0,0,0 }, // 28
    { 0,0,0,0,1,3,3,3,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,3,3,3,1,0,0,0,0 }, // 29
    { 0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0 }, // 30
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }, // 31
};

// Helper to get RGB565 color from palette index
static uint16_t snorlax_get_color(uint8_t idx) {
    switch (idx) {
        case 1: return SNORLAX_OUTLINE;
        case 2: return SNORLAX_BODY;
        case 3: return SNORLAX_BELLY;
        default: return COLOR_BLACK; // transparent = background
    }
}

// Draw the Snorlax sprite at given position with scale factor
static void lcd_draw_snorlax(int x, int y, int scale) {
    for (int row = 0; row < 32; row++) {
        for (int col = 0; col < 32; col++) {
            uint8_t idx = snorlax_sprite[row][col];
            if (idx != 0) { // Skip transparent pixels
                uint16_t color = snorlax_get_color(idx);
                lcd_fill_rect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

esp_err_t lcd_display_sleep_screen(void)
{
    lcd_display_clear();

    // Draw title "Sleeping..."
    lcd_draw_string(20, 10, "Sleeping...", COLOR_CYAN, COLOR_BLACK, 2);

    // Draw Snorlax sprite centered (32x32 at scale 3 = 96x96 pixels)
    int sprite_size = 32 * 3; // 96 pixels
    int sprite_x = (LCD_WIDTH - sprite_size) / 2;
    int sprite_y = 50;
    lcd_draw_snorlax(sprite_x, sprite_y, 3);

    // Draw animated "Zzz" with increasing sizes
    lcd_draw_string(sprite_x + sprite_size - 10, sprite_y - 5, "z", COLOR_WHITE, COLOR_BLACK, 1);
    lcd_draw_string(sprite_x + sprite_size + 5, sprite_y - 15, "Z", COLOR_WHITE, COLOR_BLACK, 1);
    lcd_draw_string(sprite_x + sprite_size + 15, sprite_y - 30, "Z", COLOR_WHITE, COLOR_BLACK, 2);

    // Footer message
    lcd_draw_string(8, LCD_HEIGHT - 30, "Press RIGHT btn", COLOR_YELLOW, COLOR_BLACK, 1);
    lcd_draw_string(8, LCD_HEIGHT - 18, "to wake up", COLOR_YELLOW, COLOR_BLACK, 1);

    return ESP_OK;
}
