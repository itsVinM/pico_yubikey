# Portable core library (always built — host tests + firmware link against this)
add_library(yk_core STATIC
  core/sha1.cpp
  core/command.cpp
)
target_include_directories(yk_core PUBLIC ${PROJECT_SOURCE_DIR}/include)

# Firmware binary (only when building with Pico SDK)
if(PICO_SDK_IMPORTED)
  add_executable(pico_yubikey
    main.cpp
    arch/rp2040/flash.cpp
  )
  target_link_libraries(pico_yubikey PRIVATE yk_core pico_stdlib hardware_uart hardware_flash)
  target_include_directories(pico_yubikey PRIVATE ${PROJECT_SOURCE_DIR}/include)
  pico_add_extra_outputs(pico_yubikey)
  pico_enable_stdio_usb(pico_yubikey 0)
  pico_enable_stdio_uart(pico_yubikey 1)
endif()
