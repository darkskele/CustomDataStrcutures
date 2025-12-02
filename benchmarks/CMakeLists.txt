cmake_minimum_required(VERSION 3.20)

include(FetchContent)

# Fetch Google Benchmark
FetchContent_Declare(
  googlebenchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.9.0
)

# Disable benchmark’s internal tests
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googlebenchmark)

# Benchmark executable
add_executable(mcds_benchmarks
    bench_b_tree.cpp
    bench_flat_map.cpp
    bench_std_map_baseline.cpp
    bench_slab.cpp
    bench_memory_pool.cpp
    bench_ring_buffer.cpp
    bench_spsc_ring_buffer.cpp
    bench_main.cpp
)

target_link_libraries(mcds_benchmarks
    PRIVATE
        mcds
        benchmark::benchmark
        benchmark::benchmark_main
)
