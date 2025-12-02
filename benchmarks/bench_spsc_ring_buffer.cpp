
#include "mcds/spsc_ring_buffer.hpp"
#include "benchmark_suites/ring_buffer_benchmarks.hpp"

DEFINE_RING_BUFFER_BENCHMARKS(SPSCRingBuffer, mcds::spsc_ring_buffer, 100'000)

namespace mcds::bench
{
    REGISTER_RING_BUFFER_BENCHMARKS(SPSCRingBuffer, SmallPayload)
    REGISTER_RING_BUFFER_BENCHMARKS(SPSCRingBuffer, MediumPayload)
    REGISTER_RING_BUFFER_BENCHMARKS(SPSCRingBuffer, LargePayload)
}
