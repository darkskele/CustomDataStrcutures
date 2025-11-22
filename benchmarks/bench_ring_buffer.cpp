#include <benchmark/benchmark.h>
#include <cstdint>
#include <random>
#include <vector>

#include "mcds/ring_buffer.hpp"
#include "benchmark_suites/data_types.hpp"

namespace mcds::bench
{

    template <typename Value, std::size_t Capacity>
    static void BM_RingBuffer_PushSequential(benchmark::State &state)
    {
        for (auto _ : state)
        {
            mcds::ring_buffer<Value, Capacity> buf;

            for (std::size_t i = 0; i < Capacity; ++i)
            {
                buf.push(make_payload<Value>(static_cast<std::uint64_t>(i)));
            }

            benchmark::DoNotOptimize(buf);
            benchmark::ClobberMemory();
        }
    }

    template <typename Value, std::size_t Capacity>
    static void BM_RingBuffer_PopSequential(benchmark::State &state)
    {
        mcds::ring_buffer<Value, Capacity> buf;

        // Initial fill (not timed)
        for (std::size_t i = 0; i < Capacity; ++i)
        {
            buf.push(make_payload<Value>(static_cast<std::uint64_t>(i)));
        }

        for (auto _ : state)
        {
            // For each element, push then pop_front
            for (std::size_t i = 0; i < Capacity; ++i)
            {
                buf.pop_front();
            }
            benchmark::DoNotOptimize(buf);
            benchmark::ClobberMemory();
        }
    }

    template <typename Value, std::size_t Capacity>
    static void BM_RingBuffer_Overwrite(benchmark::State &state)
    {
        mcds::ring_buffer<Value, Capacity> buf;

        // Fill to capacity once (not timed)
        for (std::size_t i = 0; i < Capacity; ++i)
        {
            buf.push(make_payload<Value>(static_cast<std::uint64_t>(i)));
        }

        for (auto _ : state)
        {
            for (std::size_t i = 0; i < Capacity; ++i)
            {
                auto payload = make_payload<Value>(static_cast<std::uint64_t>(i));
                buf.push(payload); // overwrites oldest when full
            }

            // Touch front/back so compiler can't assume they're unused
            benchmark::DoNotOptimize(buf.front());
            benchmark::DoNotOptimize(buf.back());
            benchmark::ClobberMemory();
        }
    }

    template <typename Value, std::size_t Capacity>
    static void BM_RingBuffer_RandomAccess(benchmark::State &state)
    {
        mcds::ring_buffer<Value, Capacity> buf;

        constexpr std::size_t count = Capacity;

        // Fill once
        for (std::size_t i = 0; i < count; ++i)
        {
            buf.push(make_payload<Value>(static_cast<std::uint64_t>(i)));
        }

        if constexpr (count == 0)
        {
            for (auto _ : state)
            {
                benchmark::ClobberMemory();
            }
            return;
        }

        // Pre generate random indices once
        std::vector<std::size_t> indices(count);
        std::mt19937_64 rng(42);
        std::uniform_int_distribution<std::size_t> dist(0, count - 1);

        for (std::size_t i = 0; i < count; ++i)
        {
            indices[i] = dist(rng);
        }

        for (auto _ : state)
        {
            for (std::size_t i = 0; i < count; ++i)
            {
                auto &v = buf[indices[i]];
                benchmark::DoNotOptimize(v);
            }
            benchmark::ClobberMemory();
        }
    }

    template <typename Value, std::size_t Capacity>
    static void BM_RingBuffer_Clear(benchmark::State &state)
    {
        mcds::ring_buffer<Value, Capacity> buf;

        constexpr std::size_t count = Capacity;

        // Fill buffer to count
        buf.clear();
        for (std::size_t i = 0; i < count; ++i)
        {
            buf.push(make_payload<Value>(static_cast<std::uint64_t>(i)));
        }
        benchmark::ClobberMemory();

        for (auto _ : state)
        {
            // Only clear() is timed
            buf.clear();
            benchmark::ClobberMemory();
        }
    }

    // SmallPayload
    BENCHMARK_TEMPLATE(BM_RingBuffer_PushSequential, SmallPayload, 1'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_PushSequential, SmallPayload, 10'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_PushSequential, SmallPayload, 100'000);

    BENCHMARK_TEMPLATE(BM_RingBuffer_PopSequential, SmallPayload, 1'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_PopSequential, SmallPayload, 10'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_PopSequential, SmallPayload, 100'000);

    BENCHMARK_TEMPLATE(BM_RingBuffer_Overwrite, SmallPayload, 1'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_Overwrite, SmallPayload, 10'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_Overwrite, SmallPayload, 100'000);

    BENCHMARK_TEMPLATE(BM_RingBuffer_RandomAccess, SmallPayload, 1'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_RandomAccess, SmallPayload, 10'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_RandomAccess, SmallPayload, 100'000);

    BENCHMARK_TEMPLATE(BM_RingBuffer_Clear, SmallPayload, 1'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_Clear, SmallPayload, 10'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_Clear, SmallPayload, 100'000);

    // MediumPayload
    BENCHMARK_TEMPLATE(BM_RingBuffer_PushSequential, MediumPayload, 1'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_PushSequential, MediumPayload, 10'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_PushSequential, MediumPayload, 100'000);

    BENCHMARK_TEMPLATE(BM_RingBuffer_PopSequential, MediumPayload, 1'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_PopSequential, MediumPayload, 10'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_PopSequential, MediumPayload, 100'000);

    BENCHMARK_TEMPLATE(BM_RingBuffer_Overwrite, MediumPayload, 1'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_Overwrite, MediumPayload, 10'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_Overwrite, MediumPayload, 100'000);

    BENCHMARK_TEMPLATE(BM_RingBuffer_RandomAccess, MediumPayload, 1'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_RandomAccess, MediumPayload, 10'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_RandomAccess, MediumPayload, 100'000);

    BENCHMARK_TEMPLATE(BM_RingBuffer_Clear, MediumPayload, 1'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_Clear, MediumPayload, 10'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_Clear, MediumPayload, 100'000);

    // LargePayload
    BENCHMARK_TEMPLATE(BM_RingBuffer_PushSequential, LargePayload, 1'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_PushSequential, LargePayload, 10'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_PushSequential, LargePayload, 100'000);

    BENCHMARK_TEMPLATE(BM_RingBuffer_PopSequential, LargePayload, 1'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_PopSequential, LargePayload, 10'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_PopSequential, LargePayload, 100'000);

    BENCHMARK_TEMPLATE(BM_RingBuffer_Overwrite, LargePayload, 1'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_Overwrite, LargePayload, 10'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_Overwrite, LargePayload, 100'000);

    BENCHMARK_TEMPLATE(BM_RingBuffer_RandomAccess, LargePayload, 1'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_RandomAccess, LargePayload, 10'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_RandomAccess, LargePayload, 100'000);

    BENCHMARK_TEMPLATE(BM_RingBuffer_Clear, LargePayload, 1'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_Clear, LargePayload, 10'000);
    BENCHMARK_TEMPLATE(BM_RingBuffer_Clear, LargePayload, 100'000);

} // namespace mcds::bench
