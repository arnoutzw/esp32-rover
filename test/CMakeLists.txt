# Test application CMakeLists.txt
# Build with: idf.py -C test build

cmake_minimum_required(VERSION 3.16)

# Include unity test framework
set(EXTRA_COMPONENT_DIRS
    ../components
    $ENV{IDF_PATH}/components/unity
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32-rover-tests)
