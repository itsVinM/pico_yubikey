add_executable(yk_tests
  test_all.cpp
  test_sha1.cpp
  test_otp.cpp
  test_storage.cpp
  test_command.cpp
  test_slot.cpp
)
target_link_libraries(yk_tests PRIVATE yk_core)
add_test(NAME yk_tests COMMAND yk_tests)
