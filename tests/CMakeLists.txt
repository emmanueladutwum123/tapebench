include(FetchContent)

set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
)
FetchContent_MakeAvailable(googletest)

add_executable(tapebench_tests
  test_synthetic_generator.cpp
)
target_link_libraries(tapebench_tests PRIVATE tapebench GTest::gtest_main)
target_compile_options(tapebench_tests PRIVATE -Wall -Wextra)

include(GoogleTest)
gtest_discover_tests(tapebench_tests)
