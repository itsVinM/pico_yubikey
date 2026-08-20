# Algo test harness — runs on emulated RP2040 via Renode
add_executable(algo_test
  main.cpp
)
target_link_libraries(algo_test PRIVATE yk_core pico_stdlib hardware_uart)
target_include_directories(algo_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
pico_add_extra_outputs(algo_test)
pico_enable_stdio_usb(algo_test 0)
pico_enable_stdio_uart(algo_test 0)
