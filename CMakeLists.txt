cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32-rover)

# Generate resource consumption report after build
add_custom_target(resource_report ALL
    COMMAND python3 ${CMAKE_SOURCE_DIR}/generate_resource_report.py ${CMAKE_BINARY_DIR}
    COMMENT "Generating resource consumption report..."
    VERBATIM
)
