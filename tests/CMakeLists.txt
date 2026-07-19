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
  test_mapped_tape.cpp
  test_bar_aggregator.cpp
  test_strategy.cpp
  test_moving_average_crossover.cpp
  test_mean_reversion.cpp
)
target_link_libraries(tapebench_tests PRIVATE tapebench GTest::gtest_main)
target_compile_options(tapebench_tests PRIVATE -Wall -Wextra)

# Separate executable: overrides global operator new/delete to verify the
# zero-copy claim on MappedTape's read path. Kept isolated so that
# instrumentation doesn't affect tapebench_tests' own allocation behavior.
add_executable(tapebench_zero_copy_tests
  test_mapped_tape_zero_copy.cpp
)
target_link_libraries(tapebench_zero_copy_tests PRIVATE tapebench GTest::gtest_main)
target_compile_options(tapebench_zero_copy_tests PRIVATE -Wall -Wextra)

include(GoogleTest)
gtest_discover_tests(tapebench_tests)
gtest_discover_tests(tapebench_zero_copy_tests)
