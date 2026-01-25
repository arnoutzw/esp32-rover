# Build info component - provides build_info.h with version and commit info
idf_component_register(
    INCLUDE_DIRS "include"
)

# Generate build_info.h with git commit hash, timestamp, and version
set(BUILD_INFO_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/build_info.h")
add_custom_command(
    OUTPUT "${BUILD_INFO_HEADER}"
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/generate_build_info.sh "${BUILD_INFO_HEADER}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..
    COMMENT "Generating build_info.h"
    VERBATIM
)
add_custom_target(generate_build_info ALL DEPENDS "${BUILD_INFO_HEADER}")
