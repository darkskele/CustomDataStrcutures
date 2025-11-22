#include <benchmark/benchmark.h>
#include <vector>
#include <random>
#include <algorithm>

#include "mcds/memory/slab.hpp"
#include "benchmark_suites/data_types.hpp"

namespace mcds::bench
{

    using namespace memory;

    template <typename PayloadType, typename IndexType>
    static void BM_Slab_Allocate_Single(benchmark::State &state)
    {
        Slab<PayloadType, IndexType> slab;
        uint64_t seed = 0;

        for (auto _ : state)
        {
            auto payload = make_payload<PayloadType>(seed++);
            auto idx = slab.allocate(payload);
            benchmark::DoNotOptimize(idx);
            benchmark::ClobberMemory();
            slab.deallocate(idx);
        }

        state.SetItemsProcessed(state.iterations());
        state.SetLabel(std::to_string(sizeof(PayloadType)) + "B");
    }

    template <typename PayloadType, typename IndexType>
    static void BM_Slab_AllocateOnly(benchmark::State &state)
    {
        Slab<PayloadType, IndexType> slab;
        uint64_t seed = 0;
        std::vector<IndexType> allocated_indices;
        allocated_indices.reserve(slab.capacity());

        for (auto _ : state)
        {
            state.PauseTiming();
            // Reset slab by deallocating everything
            for (auto idx : allocated_indices)
            {
                slab.deallocate(idx);
            }
            allocated_indices.clear();
            state.ResumeTiming();

            // Benchmark: allocate as many as possible
            while (slab.has_free())
            {
                auto payload = make_payload<PayloadType>(seed++);
                auto idx = slab.allocate(payload);
                benchmark::DoNotOptimize(idx);
                allocated_indices.push_back(idx);
            }
            benchmark::ClobberMemory();
        }

        state.SetItemsProcessed(state.iterations() * slab.capacity());
        state.SetLabel(std::to_string(sizeof(PayloadType)) + "B");
    }

    template <typename PayloadType, typename IndexType>
    static void BM_Slab_DeallocateOnly(benchmark::State &state)
    {
        Slab<PayloadType, IndexType> slab;
        std::vector<IndexType> indices;
        indices.reserve(slab.capacity());

        for (auto _ : state)
        {
            state.PauseTiming();
            // Fill slab
            indices.clear();
            for (uint64_t i = 0; i < slab.capacity(); ++i)
            {
                auto payload = make_payload<PayloadType>(i);
                indices.push_back(slab.allocate(payload));
            }
            state.ResumeTiming();

            // Benchmark: deallocate all
            for (auto idx : indices)
            {
                slab.deallocate(idx);
                benchmark::ClobberMemory();
            }
        }

        state.SetItemsProcessed(state.iterations() * slab.capacity());
        state.SetLabel(std::to_string(sizeof(PayloadType)) + "B");
    }

    template <typename PayloadType, typename IndexType>
    static void BM_Slab_RandomAccess(benchmark::State &state)
    {
        Slab<PayloadType, IndexType> slab;
        const size_t num_objects = slab.capacity();

        std::vector<size_t> indices;
        indices.reserve(num_objects);

        // Pre-allocate objects
        for (size_t i = 0; i < num_objects; ++i)
        {
            auto payload = make_payload<PayloadType>(static_cast<int>(i));
            indices.push_back(slab.allocate(payload));
        }

        // Shuffle access order once
        std::mt19937 gen(42);
        std::shuffle(indices.begin(), indices.end(), gen);

        for (auto _ : state)
        {
            // Walk the slab in random order
            for (auto idx : indices)
            {
                auto &obj = slab[idx];
                benchmark::DoNotOptimize(obj);
            }
            benchmark::ClobberMemory();
        }

        state.SetItemsProcessed(state.iterations() * num_objects);
        state.SetLabel(std::to_string(sizeof(PayloadType)) +
                       "B, objects=" + std::to_string(num_objects));
    }

    template <typename PayloadType, typename IndexType>
    static void BM_Slab_SequentialAllocation(benchmark::State &state)
    {
        for (auto _ : state)
        {
            // Setup
            state.PauseTiming();
            Slab<PayloadType, IndexType> slab;
            state.ResumeTiming();

            // Fill slab sequentially
            for (uint64_t i = 0; i < slab.capacity(); ++i)
            {
                auto idx = slab.allocate(make_payload<PayloadType>(i));
                benchmark::DoNotOptimize(idx);

            }
            benchmark::ClobberMemory();
        }

        state.SetItemsProcessed(state.iterations() * static_cast<size_t>(std::numeric_limits<IndexType>::max()) + 1ULL);
        state.SetLabel(std::to_string(sizeof(PayloadType)) + "B");
    }

    template <typename PayloadType, typename IndexType>
    static void BM_Slab_SequentialDeallocation(benchmark::State &state)
    {
        Slab<PayloadType, IndexType> slab;
        std::vector<size_t> indices;
        indices.reserve(slab.capacity());

        for (auto _ : state)
        {
            state.PauseTiming();
            // Fill slab sequentially
            indices.clear();
            for (uint64_t i = 0; i < slab.capacity(); ++i)
            {
                auto payload = make_payload<PayloadType>(i);
                indices.push_back(slab.allocate(payload));
            }
            state.ResumeTiming();

            // Deallocate sequentially
            for (auto idx : indices)
            {
                slab.deallocate(idx);
            }
            benchmark::ClobberMemory();
        }

        state.SetItemsProcessed(state.iterations() * slab.capacity());
        state.SetLabel(std::to_string(sizeof(PayloadType)) + "B");
    }

    template <typename PayloadType, typename IndexType>
    static void BM_Slab_RandomDeallocation(benchmark::State &state)
    {
        Slab<PayloadType, IndexType> slab;
        std::vector<size_t> indices;
        indices.reserve(slab.capacity());

        for (auto _ : state)
        {
            state.PauseTiming();
            // Fill slab
            indices.clear();
            for (uint64_t i = 0; i < slab.capacity(); ++i)
            {
                auto payload = make_payload<PayloadType>(i);
                indices.push_back(slab.allocate(payload));
            }

            // Shuffle for random deallocation
            std::mt19937 gen(42);
            std::shuffle(indices.begin(), indices.end(), gen);
            state.ResumeTiming();

            // Deallocate randomly
            for (auto idx : indices)
            {
                slab.deallocate(idx);
            }
            benchmark::ClobberMemory();
        }

        state.SetItemsProcessed(state.iterations() * slab.capacity());
        state.SetLabel(std::to_string(sizeof(PayloadType)) + "B");
    }

    template <typename PayloadType, typename IndexType>
    static void BM_Baseline_Vector_PushBack(benchmark::State &state)
    {
        uint64_t seed = 0;
        size_t num_objects = static_cast<size_t>(std::numeric_limits<IndexType>::max()) + 1ULL;;

        for (auto _ : state)
        {
            state.PauseTiming();
            std::vector<PayloadType> vec;
            vec.reserve(num_objects);
            state.ResumeTiming();

            for (int i = 0; i < num_objects; ++i)
            {
                auto payload = make_payload<PayloadType>(seed++);
                vec.push_back(payload);
            }
            benchmark::DoNotOptimize(vec.data());
            benchmark::ClobberMemory();
        }

        state.SetItemsProcessed(state.iterations() * num_objects);
        state.SetLabel(std::to_string(sizeof(PayloadType)) + "B");
    }

    // Small Payload
    BENCHMARK(BM_Slab_Allocate_Single<SmallPayload, uint8_t>);
    BENCHMARK(BM_Slab_Allocate_Single<SmallPayload, uint16_t>);

    BENCHMARK(BM_Slab_AllocateOnly<SmallPayload, uint8_t>);
    BENCHMARK(BM_Slab_AllocateOnly<SmallPayload, uint16_t>);

    BENCHMARK(BM_Slab_DeallocateOnly<SmallPayload, uint8_t>);
    BENCHMARK(BM_Slab_DeallocateOnly<SmallPayload, uint16_t>);

    BENCHMARK(BM_Slab_RandomAccess<SmallPayload, uint8_t>);
    BENCHMARK(BM_Slab_RandomAccess<SmallPayload, uint16_t>);

    BENCHMARK(BM_Slab_SequentialAllocation<SmallPayload, uint8_t>);
    BENCHMARK(BM_Slab_SequentialAllocation<SmallPayload, uint16_t>);

    BENCHMARK(BM_Slab_SequentialDeallocation<SmallPayload, uint8_t>);
    BENCHMARK(BM_Slab_SequentialDeallocation<SmallPayload, uint16_t>);

    BENCHMARK(BM_Slab_RandomDeallocation<SmallPayload, uint8_t>);
    BENCHMARK(BM_Slab_RandomDeallocation<SmallPayload, uint16_t>);

    BENCHMARK(BM_Baseline_Vector_PushBack<SmallPayload, uint8_t>);
    BENCHMARK(BM_Baseline_Vector_PushBack<SmallPayload, uint16_t>);

    // Medium Payload
    BENCHMARK(BM_Slab_Allocate_Single<MediumPayload, uint8_t>);
    BENCHMARK(BM_Slab_Allocate_Single<MediumPayload, uint16_t>);

    BENCHMARK(BM_Slab_AllocateOnly<MediumPayload, uint8_t>);
    BENCHMARK(BM_Slab_AllocateOnly<MediumPayload, uint16_t>);

    BENCHMARK(BM_Slab_DeallocateOnly<MediumPayload, uint8_t>);
    BENCHMARK(BM_Slab_DeallocateOnly<MediumPayload, uint16_t>);

    BENCHMARK(BM_Slab_RandomAccess<MediumPayload, uint8_t>);
    BENCHMARK(BM_Slab_RandomAccess<MediumPayload, uint16_t>);

    BENCHMARK(BM_Slab_SequentialAllocation<MediumPayload, uint8_t>);
    BENCHMARK(BM_Slab_SequentialAllocation<MediumPayload, uint16_t>);

    BENCHMARK(BM_Slab_SequentialDeallocation<MediumPayload, uint8_t>);
    BENCHMARK(BM_Slab_SequentialDeallocation<MediumPayload, uint16_t>);

    BENCHMARK(BM_Slab_RandomDeallocation<MediumPayload, uint8_t>);
    BENCHMARK(BM_Slab_RandomDeallocation<MediumPayload, uint16_t>);

    BENCHMARK(BM_Baseline_Vector_PushBack<MediumPayload, uint8_t>);
    BENCHMARK(BM_Baseline_Vector_PushBack<MediumPayload, uint16_t>);

    // Large Payload
    BENCHMARK(BM_Slab_Allocate_Single<LargePayload, uint8_t>);
    BENCHMARK(BM_Slab_Allocate_Single<LargePayload, uint16_t>);

    BENCHMARK(BM_Slab_AllocateOnly<LargePayload, uint8_t>);
    BENCHMARK(BM_Slab_AllocateOnly<LargePayload, uint16_t>);

    BENCHMARK(BM_Slab_DeallocateOnly<LargePayload, uint8_t>);
    BENCHMARK(BM_Slab_DeallocateOnly<LargePayload, uint16_t>);

    BENCHMARK(BM_Slab_RandomAccess<LargePayload, uint8_t>);
    BENCHMARK(BM_Slab_RandomAccess<LargePayload, uint16_t>);

    BENCHMARK(BM_Slab_SequentialAllocation<LargePayload, uint8_t>);
    BENCHMARK(BM_Slab_SequentialAllocation<LargePayload, uint16_t>);

    BENCHMARK(BM_Slab_SequentialDeallocation<LargePayload, uint8_t>);
    BENCHMARK(BM_Slab_SequentialDeallocation<LargePayload, uint16_t>);

    BENCHMARK(BM_Slab_RandomDeallocation<LargePayload, uint8_t>);
    BENCHMARK(BM_Slab_RandomDeallocation<LargePayload, uint16_t>);

    BENCHMARK(BM_Baseline_Vector_PushBack<LargePayload, uint8_t>);
    BENCHMARK(BM_Baseline_Vector_PushBack<LargePayload, uint16_t>);

} // namespace mcds::memory::bench
