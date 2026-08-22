cmake_minimum_required(VERSION 3.20)

# ---------------------------------------------------------------------------
# pico-yubikey: OTP-mode USB token (HOTP/TOTP/challenge-response) on RP2040.
#
# Two build flavors:
#   * Host (default): pure portable core + unit tests, runs on the build machine.
#   * Firmware:       add -DPICO_SDK_PATH=<sdk> to build the RP2040 target.
#
# The Pico SDK must load its toolchain preload BEFORE project(), so we include
# the SDK import early. Host-only builds skip it and use the native toolchain.
# ---------------------------------------------------------------------------
if(NOT DEFINED PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()
set(PICO_SDK_IMPORTED OFF)
if(DEFINED PICO_SDK_PATH AND EXISTS "${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
    include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
    set(PICO_SDK_IMPORTED ON)
endif()

project(pico-yubikey VERSION 0.1.0 LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

if(PICO_SDK_IMPORTED)
    pico_sdk_init()
endif()

add_subdirectory(src)
if(NOT CMAKE_CROSSCOMPILING)
    enable_testing()
    add_subdirectory(tests)
endif()
