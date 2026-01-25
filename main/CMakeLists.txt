# Allow target selection via environment variable or CMake variable
# Usage: ROVER_TARGET=esp32cam idf.py build
# Or: idf.py build -DROVER_TARGET=esp32cam
if(DEFINED ENV{ROVER_TARGET})
    set(ROVER_TARGET $ENV{ROVER_TARGET})
endif()

# Default to TTGO if not specified
if(NOT DEFINED ROVER_TARGET OR ROVER_TARGET STREQUAL "")
    set(ROVER_TARGET "ttgo")
endif()

# Base requirements for all targets
set(COMPONENT_REQUIRES
    driver
    esp_wifi
    esp_http_server
    nvs_flash
    esp_psram
    esp_adc
    esp_system
    web_server
    lcd_display
    mqtt_service
    log_buffer
    lwip
    app_update
    resource_guard
)

# Add camera component only for ESP32-CAM builds
if(ROVER_TARGET STREQUAL "esp32cam")
    list(APPEND COMPONENT_REQUIRES camera)
    message(STATUS "Building for ESP32-CAM (camera enabled)")
elseif(ROVER_TARGET STREQUAL "ttgo")
    message(STATUS "Building for TTGO T-Display (no camera)")
else()
    message(FATAL_ERROR "Invalid ROVER_TARGET: ${ROVER_TARGET}. Must be 'esp32cam' or 'ttgo'")
endif()

idf_component_register(
    SRCS
        "main.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        ${COMPONENT_REQUIRES}
)

if(ROVER_TARGET STREQUAL "esp32cam")
    target_compile_definitions(${COMPONENT_LIB} PUBLIC ROVER_TARGET_ESP32CAM=1)
elseif(ROVER_TARGET STREQUAL "ttgo")
    target_compile_definitions(${COMPONENT_LIB} PUBLIC ROVER_TARGET_TTGO=1)
endif()

# REQ-38: JTAG Debug Mode - frees GPIO 12-15 for JTAG debugging
# Usage: JTAG_DEBUG=1 ROVER_TARGET=esp32cam idf.py build
# Or: idf.py build -DROVER_TARGET=esp32cam -DJTAG_DEBUG=1
if(DEFINED ENV{JTAG_DEBUG})
    set(JTAG_DEBUG $ENV{JTAG_DEBUG})
endif()

if(JTAG_DEBUG)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC ENABLE_JTAG_DEBUG=1)
    message(STATUS "JTAG debug mode ENABLED - GPIO 12-15 available for JTAG")
endif()

# Generate build_info.h with git commit hash and timestamp
set(BUILD_INFO_HEADER "${CMAKE_CURRENT_BINARY_DIR}/build_info.h")
add_custom_command(
    OUTPUT "${BUILD_INFO_HEADER}"
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../generate_build_info.sh "${BUILD_INFO_HEADER}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    COMMENT "Generating build_info.h"
    VERBATIM
)
add_custom_target(generate_build_info DEPENDS "${BUILD_INFO_HEADER}")
add_dependencies(${COMPONENT_LIB} generate_build_info)
target_include_directories(${COMPONENT_LIB} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
