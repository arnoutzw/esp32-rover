idf_component_register(
    SRCS
        "main.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        driver
        esp_wifi
        esp_http_server
        nvs_flash
        esp_psram
        esp_adc
        esp_system
        as5600
        bldc_motor
        servo_control
        camera
        web_server
        lcd_display
        mqtt_service
        log_buffer
        lwip
        app_update
        resource_guard
        status_led
        sd_card
)

# Allow target selection via environment variable or CMake variable
# Usage: ROVER_TARGET=esp32cam idf.py build
# Or: idf.py build -DROVER_TARGET=esp32cam
if(DEFINED ENV{ROVER_TARGET})
    set(ROVER_TARGET $ENV{ROVER_TARGET})
endif()

if(ROVER_TARGET STREQUAL "esp32cam")
    target_compile_definitions(${COMPONENT_LIB} PUBLIC ROVER_TARGET_ESP32CAM=1)
    message(STATUS "Building for ESP32-CAM (camera enabled)")
elseif(ROVER_TARGET STREQUAL "ttgo")
    target_compile_definitions(${COMPONENT_LIB} PUBLIC ROVER_TARGET_TTGO=1)
    message(STATUS "Building for TTGO T-Display (no camera)")
else()
    # Default to TTGO if not specified
    target_compile_definitions(${COMPONENT_LIB} PUBLIC ROVER_TARGET_TTGO=1)
    message(STATUS "No target specified, defaulting to TTGO T-Display")
endif()

# REQ-38: JTAG Debug Mode - disables motor control to free GPIO 12-15 for JTAG
# Usage: JTAG_DEBUG=1 ROVER_TARGET=esp32cam idf.py build
# Or: idf.py build -DROVER_TARGET=esp32cam -DJTAG_DEBUG=1
if(DEFINED ENV{JTAG_DEBUG})
    set(JTAG_DEBUG $ENV{JTAG_DEBUG})
endif()

if(JTAG_DEBUG)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC ENABLE_JTAG_DEBUG=1)
    message(STATUS "JTAG debug mode ENABLED - motor control DISABLED (GPIO 12-15 available for JTAG)")
endif()

# REQ-40: SD Card Debug Mode - enables SD card in 1-bit mode, disables motor control
# SD card shares pins with motor, so they are mutually exclusive
# Usage: SD_CARD_DEBUG=1 ROVER_TARGET=esp32cam idf.py build
# Or: idf.py build -DROVER_TARGET=esp32cam -DSD_CARD_DEBUG=1
if(DEFINED ENV{SD_CARD_DEBUG})
    set(SD_CARD_DEBUG $ENV{SD_CARD_DEBUG})
endif()

if(SD_CARD_DEBUG)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC ENABLE_SD_CARD=1)
    # SD card mode also implies JTAG mode disabled motor
    target_compile_definitions(${COMPONENT_LIB} PUBLIC ENABLE_JTAG_DEBUG=1)
    message(STATUS "SD Card debug mode ENABLED - motor control DISABLED (GPIO 14-15 used by SD card)")
endif()
