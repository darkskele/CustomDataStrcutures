#include "mcds/flat_map.hpp"
#include "benchmark_suites/associative_container_benchmarks.hpp"

// Need a template alias that matches the expected interface
namespace mcds
{
    template <typename Key, typename Value>
    using FlatMapBench = FlatMap<Key, Value, 200'000>; // Large enough for benchmarks
}

// Define all benchmark functions for FlatMap
DEFINE_ASSOCIATIVE_CONTAINER_BENCHMARKS(FlatMap, mcds::FlatMapBench)

namespace mcds::bench
{
    // Register all benchmarks for each payload size
    REGISTER_ASSOCIATIVE_CONTAINER_BENCHMARKS(FlatMap, SmallPayload)
    REGISTER_ASSOCIATIVE_CONTAINER_BENCHMARKS(FlatMap, MediumPayload)
    REGISTER_ASSOCIATIVE_CONTAINER_BENCHMARKS(FlatMap, LargePayload)
}
