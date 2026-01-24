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
        as5600
        bldc_motor
        servo_control
        camera
        web_server
        lcd_display
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
