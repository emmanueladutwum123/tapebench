if(TB_BUILD_BENCHMARKS)
  # Deliberately no sanitizers on any target below -- ASan/UBSan/TSan overhead
  # would make the reported throughput numbers meaningless. Same reasoning
  # the sibling liquibook-x repo's bench/CMakeLists.txt uses.

  add_executable(tapebench_bench_dispatch bench_dispatch.cpp virtual_strategy.cpp)
  target_link_libraries(tapebench_bench_dispatch PRIVATE tapebench benchmark::benchmark)
  target_compile_options(tapebench_bench_dispatch PRIVATE -Wall -Wextra)

  add_executable(tapebench_bench_tick_generation bench_tick_generation.cpp)
  target_link_libraries(tapebench_bench_tick_generation PRIVATE tapebench benchmark::benchmark)
  target_compile_options(tapebench_bench_tick_generation PRIVATE -Wall -Wextra)

  add_executable(tapebench_bench_mapped_tape bench_mapped_tape.cpp)
  target_link_libraries(tapebench_bench_mapped_tape PRIVATE tapebench benchmark::benchmark)
  target_compile_options(tapebench_bench_mapped_tape PRIVATE -Wall -Wextra)

  add_executable(tapebench_bench_bar_aggregation bench_bar_aggregation.cpp)
  target_link_libraries(tapebench_bench_bar_aggregation PRIVATE tapebench benchmark::benchmark)
  target_compile_options(tapebench_bench_bar_aggregation PRIVATE -Wall -Wextra)

  add_executable(tapebench_bench_simulate bench_simulate.cpp)
  target_link_libraries(tapebench_bench_simulate PRIVATE tapebench benchmark::benchmark)
  target_compile_options(tapebench_bench_simulate PRIVATE -Wall -Wextra)

  add_executable(tapebench_bench_parallel_grid bench_parallel_grid.cpp)
  target_link_libraries(tapebench_bench_parallel_grid PRIVATE tapebench benchmark::benchmark)
  target_compile_options(tapebench_bench_parallel_grid PRIVATE -Wall -Wextra)
endif()
