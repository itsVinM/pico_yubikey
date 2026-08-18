add_library(yk_core STATIC
  core/sha1.cpp
  core/command.cpp
)
target_include_directories(yk_core PUBLIC ${PROJECT_SOURCE_DIR}/include)
