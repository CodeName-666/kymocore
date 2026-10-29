# SPDX-License-Identifier: MIT
# ESP-IDF component build (ESP Component Registry, see idf_component.yml).
# PlatformIO and the Arduino IDE ignore this file; plain CMake projects add
# src/*.cpp themselves (see docs/GUIDE.md).
if(NOT COMMAND idf_component_register)
    message(FATAL_ERROR "KymoCore's root CMakeLists.txt is an ESP-IDF component. "
                        "For plain CMake, compile src/*.cpp and add src/ to the include path.")
endif()

file(GLOB_RECURSE KYMO_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp")
idf_component_register(SRCS ${KYMO_SOURCES}
                       INCLUDE_DIRS "src")
