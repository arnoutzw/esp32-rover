# Build info component - provides build_info.h with version and commit info
idf_component_register(
    INCLUDE_DIRS "include"
)

# Project root is 3 levels up from this component: components/build_info -> firmware -> esp32-rover-firmware
set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

# Generate build_info.h with git commit hash, timestamp, and version
set(BUILD_INFO_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/build_info.h")
add_custom_command(
    OUTPUT "${BUILD_INFO_HEADER}"
    COMMAND ${PROJECT_ROOT}/scripts/generate_build_info.sh "${BUILD_INFO_HEADER}"
    WORKING_DIRECTORY ${PROJECT_ROOT}
    COMMENT "Generating build_info.h"
    VERBATIM
)
add_custom_target(generate_build_info ALL DEPENDS "${BUILD_INFO_HEADER}")
