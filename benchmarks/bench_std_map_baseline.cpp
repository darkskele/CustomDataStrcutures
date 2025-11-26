#include "baselines/std_map_wrapper.hpp"
#include "benchmark_suites/associative_container_benchmarks.hpp"

DEFINE_ASSOCIATIVE_CONTAINER_BENCHMARKS(StdMap, mcds::bench::baselines::StdMapWrapper)

namespace mcds::bench
{

    // Register all benchmarks
    REGISTER_ASSOCIATIVE_CONTAINER_BENCHMARKS(StdMap, SmallPayload)
    REGISTER_ASSOCIATIVE_CONTAINER_BENCHMARKS(StdMap, MediumPayload)
    REGISTER_ASSOCIATIVE_CONTAINER_BENCHMARKS(StdMap, LargePayload)

} // namespace mcds::bench
