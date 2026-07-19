cmake_minimum_required(VERSION 3.16)
project(tapebench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

option(TB_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(TB_ENABLE_ASAN "Build with AddressSanitizer" OFF)
option(TB_ENABLE_UBSAN "Build with UndefinedBehaviorSanitizer" OFF)
option(TB_BUILD_TESTS "Build the GoogleTest unit test suite" ON)

add_library(tapebench STATIC
  src/synthetic_generator.cpp
  src/tape_writer.cpp
  src/mapped_tape.cpp
  src/bar_aggregator.cpp
  src/strategies/moving_average_crossover.cpp
  src/strategies/mean_reversion.cpp
)
target_include_directories(tapebench PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(tapebench PUBLIC cxx_std_20)

set(TB_WARNING_FLAGS -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)
if(TB_WARNINGS_AS_ERRORS)
  list(APPEND TB_WARNING_FLAGS -Werror)
endif()
target_compile_options(tapebench PRIVATE ${TB_WARNING_FLAGS})

if(TB_ENABLE_ASAN)
  target_compile_options(tapebench PUBLIC -fsanitize=address -fno-omit-frame-pointer -g)
  target_link_options(tapebench PUBLIC -fsanitize=address)
endif()
if(TB_ENABLE_UBSAN)
  target_compile_options(tapebench PUBLIC -fsanitize=undefined -fno-omit-frame-pointer -g)
  target_link_options(tapebench PUBLIC -fsanitize=undefined)
endif()

add_executable(tapebench_demo src/main.cpp)
target_link_libraries(tapebench_demo PRIVATE tapebench)
target_compile_options(tapebench_demo PRIVATE ${TB_WARNING_FLAGS})

if(TB_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
