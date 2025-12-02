#include "mcds/ring_buffer.hpp"
#include "benchmark_suites/ring_buffer_benchmarks.hpp"

DEFINE_RING_BUFFER_BENCHMARKS(RingBuffer, mcds::ring_buffer, 100'000)

namespace mcds::bench
{
    REGISTER_RING_BUFFER_BENCHMARKS(RingBuffer, SmallPayload)
    REGISTER_RING_BUFFER_BENCHMARKS(RingBuffer, MediumPayload)
    REGISTER_RING_BUFFER_BENCHMARKS(RingBuffer, LargePayload)
}
