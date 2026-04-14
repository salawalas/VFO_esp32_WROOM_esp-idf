cmake_minimum_required(VERSION 3.16.0)

set(EXTRA_COMPONENT_DIRS src)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(vfo_pio)

# --- build type ---
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# --- version generator ---
add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/include/version.h
    COMMAND ${CMAKE_COMMAND} -E env BUILD_TYPE=${CMAKE_BUILD_TYPE} python ${CMAKE_SOURCE_DIR}/scripts/gen_version_idf.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS
        ${CMAKE_SOURCE_DIR}/scripts/gen_version_idf.py
)

add_custom_target(gen_version DEPENDS ${CMAKE_SOURCE_DIR}/include/version.h)

add_dependencies(${PROJECT_NAME}.elf gen_version)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(APP_DEBUG=1)
else()
    add_compile_definitions(APP_DEBUG=0)
endif()

execute_process(
    COMMAND git describe --tags --dirty --always
    OUTPUT_VARIABLE GIT_DESC
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_compile_definitions(GIT_DESC="${GIT_DESC}")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(LOG_LOCAL_LEVEL=ESP_LOG_DEBUG)
else()
    add_compile_definitions(LOG_LOCAL_LEVEL=ESP_LOG_INFO)
endif()