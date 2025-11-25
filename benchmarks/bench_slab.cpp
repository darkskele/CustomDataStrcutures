#include <benchmark/benchmark.h>
#include <vector>
#include <random>
#include <algorithm>

#include "mcds/memory/slab.hpp"
#include "benchmark_suites/data_types.hpp"

namespace mcds::bench
{

    using namespace memory;

    template <typename PayloadType, size_t CAPACITY>
    static void BM_Slab_Allocate_Single(benchmark::State &state)
    {
        slab<PayloadType, CAPACITY> slab;
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

    template <typename PayloadType, size_t CAPACITY>
    static void BM_Slab_AllocateOnly(benchmark::State &state)
    {
        slab<PayloadType, CAPACITY> slab;
        uint64_t seed = 0;
        std::vector<size_t> allocated_indices;
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

    template <typename PayloadType, size_t CAPACITY>
    static void BM_Slab_DeallocateOnly(benchmark::State &state)
    {
        slab<PayloadType, CAPACITY> slab;
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

    template <typename PayloadType, size_t CAPACITY>
    static void BM_Slab_RandomAccess(benchmark::State &state)
    {
        slab<PayloadType, CAPACITY> slab;
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

    template <typename PayloadType, size_t CAPACITY>
    static void BM_Slab_SequentialAllocation(benchmark::State &state)
    {
        for (auto _ : state)
        {
            // Setup
            state.PauseTiming();
            slab<PayloadType, CAPACITY> slab;
            state.ResumeTiming();

            // Fill slab sequentially
            for (uint64_t i = 0; i < slab.capacity(); ++i)
            {
                auto idx = slab.allocate(make_payload<PayloadType>(i));
                benchmark::DoNotOptimize(idx);

            }
            benchmark::ClobberMemory();
        }

        state.SetItemsProcessed(state.iterations() * CAPACITY);
        state.SetLabel(std::to_string(sizeof(PayloadType)) + "B");
    }

    template <typename PayloadType, size_t CAPACITY>
    static void BM_Slab_SequentialDeallocation(benchmark::State &state)
    {
        slab<PayloadType, CAPACITY> slab;
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

    template <typename PayloadType, size_t CAPACITY>
    static void BM_Slab_RandomDeallocation(benchmark::State &state)
    {
        slab<PayloadType, CAPACITY> slab;
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

    template <typename PayloadType, size_t CAPACITY>
    static void BM_Baseline_Vector_PushBack(benchmark::State &state)
    {
        uint64_t seed = 0;
        size_t num_objects = CAPACITY;

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
    BENCHMARK(BM_Slab_Allocate_Single<SmallPayload, 1'000>);
    BENCHMARK(BM_Slab_Allocate_Single<SmallPayload, 10'000>);
    BENCHMARK(BM_Slab_Allocate_Single<SmallPayload, 100'000>);

    BENCHMARK(BM_Slab_AllocateOnly<SmallPayload, 1'000>);
    BENCHMARK(BM_Slab_AllocateOnly<SmallPayload, 10'000>);
    BENCHMARK(BM_Slab_AllocateOnly<SmallPayload, 100'000>);

    BENCHMARK(BM_Slab_DeallocateOnly<SmallPayload, 1'000>);
    BENCHMARK(BM_Slab_DeallocateOnly<SmallPayload, 10'000>);
    BENCHMARK(BM_Slab_DeallocateOnly<SmallPayload, 100'000>);

    BENCHMARK(BM_Slab_RandomAccess<SmallPayload, 1'000>);
    BENCHMARK(BM_Slab_RandomAccess<SmallPayload, 10'000>);
    BENCHMARK(BM_Slab_RandomAccess<SmallPayload, 100'000>);

    BENCHMARK(BM_Slab_SequentialAllocation<SmallPayload, 1'000>);
    BENCHMARK(BM_Slab_SequentialAllocation<SmallPayload, 10'000>);
    BENCHMARK(BM_Slab_SequentialAllocation<SmallPayload, 100'000>);

    BENCHMARK(BM_Slab_SequentialDeallocation<SmallPayload, 1'000>);
    BENCHMARK(BM_Slab_SequentialDeallocation<SmallPayload, 10'000>);
    BENCHMARK(BM_Slab_SequentialDeallocation<SmallPayload, 100'000>);

    BENCHMARK(BM_Slab_RandomDeallocation<SmallPayload, 1'000>);
    BENCHMARK(BM_Slab_RandomDeallocation<SmallPayload, 10'000>);
    BENCHMARK(BM_Slab_RandomDeallocation<SmallPayload, 100'000>);

    BENCHMARK(BM_Baseline_Vector_PushBack<SmallPayload, 1'000>);
    BENCHMARK(BM_Baseline_Vector_PushBack<SmallPayload, 10'000>);
    BENCHMARK(BM_Baseline_Vector_PushBack<SmallPayload, 100'000>);

    // Medium Payload
    BENCHMARK(BM_Slab_Allocate_Single<MediumPayload, 1'000>);
    BENCHMARK(BM_Slab_Allocate_Single<MediumPayload, 10'000>);
    BENCHMARK(BM_Slab_Allocate_Single<MediumPayload, 100'000>);

    BENCHMARK(BM_Slab_AllocateOnly<MediumPayload, 1'000>);
    BENCHMARK(BM_Slab_AllocateOnly<MediumPayload, 10'000>);
    BENCHMARK(BM_Slab_AllocateOnly<MediumPayload, 100'000>);

    BENCHMARK(BM_Slab_DeallocateOnly<MediumPayload, 1'000>);
    BENCHMARK(BM_Slab_DeallocateOnly<MediumPayload, 10'000>);
    BENCHMARK(BM_Slab_DeallocateOnly<MediumPayload, 100'000>);

    BENCHMARK(BM_Slab_RandomAccess<MediumPayload, 1'000>);
    BENCHMARK(BM_Slab_RandomAccess<MediumPayload, 10'000>);
    BENCHMARK(BM_Slab_RandomAccess<MediumPayload, 100'000>);

    BENCHMARK(BM_Slab_SequentialAllocation<MediumPayload, 1'000>);
    BENCHMARK(BM_Slab_SequentialAllocation<MediumPayload, 10'000>);
    BENCHMARK(BM_Slab_SequentialAllocation<MediumPayload, 100'000>);

    BENCHMARK(BM_Slab_SequentialDeallocation<MediumPayload, 1'000>);
    BENCHMARK(BM_Slab_SequentialDeallocation<MediumPayload, 10'000>);
    BENCHMARK(BM_Slab_SequentialDeallocation<MediumPayload, 100'000>);

    BENCHMARK(BM_Slab_RandomDeallocation<MediumPayload, 1'000>);
    BENCHMARK(BM_Slab_RandomDeallocation<MediumPayload, 10'000>);
    BENCHMARK(BM_Slab_RandomDeallocation<MediumPayload, 100'000>);

    BENCHMARK(BM_Baseline_Vector_PushBack<MediumPayload, 1'000>);
    BENCHMARK(BM_Baseline_Vector_PushBack<MediumPayload, 10'000>);
    BENCHMARK(BM_Baseline_Vector_PushBack<MediumPayload, 100'000>);

    // Large Payload
    BENCHMARK(BM_Slab_Allocate_Single<LargePayload, 1'000>);
    BENCHMARK(BM_Slab_Allocate_Single<LargePayload, 10'000>);
    BENCHMARK(BM_Slab_Allocate_Single<LargePayload, 100'000>);

    BENCHMARK(BM_Slab_AllocateOnly<LargePayload, 1'000>);
    BENCHMARK(BM_Slab_AllocateOnly<LargePayload, 10'000>);
    BENCHMARK(BM_Slab_AllocateOnly<LargePayload, 100'000>);

    BENCHMARK(BM_Slab_DeallocateOnly<LargePayload, 1'000>);
    BENCHMARK(BM_Slab_DeallocateOnly<LargePayload, 10'000>);
    BENCHMARK(BM_Slab_DeallocateOnly<LargePayload, 100'000>);

    BENCHMARK(BM_Slab_RandomAccess<LargePayload, 1'000>);
    BENCHMARK(BM_Slab_RandomAccess<LargePayload, 10'000>);
    BENCHMARK(BM_Slab_RandomAccess<LargePayload, 100'000>);

    BENCHMARK(BM_Slab_SequentialAllocation<LargePayload, 1'000>);
    BENCHMARK(BM_Slab_SequentialAllocation<LargePayload, 10'000>);
    BENCHMARK(BM_Slab_SequentialAllocation<LargePayload, 100'000>);

    BENCHMARK(BM_Slab_SequentialDeallocation<LargePayload, 1'000>);
    BENCHMARK(BM_Slab_SequentialDeallocation<LargePayload, 10'000>);
    BENCHMARK(BM_Slab_SequentialDeallocation<LargePayload, 100'000>);

    BENCHMARK(BM_Slab_RandomDeallocation<LargePayload, 1'000>);
    BENCHMARK(BM_Slab_RandomDeallocation<LargePayload, 10'000>);
    BENCHMARK(BM_Slab_RandomDeallocation<LargePayload, 100'000>);

    BENCHMARK(BM_Baseline_Vector_PushBack<LargePayload, 1'000>);
    BENCHMARK(BM_Baseline_Vector_PushBack<LargePayload, 10'000>);
    BENCHMARK(BM_Baseline_Vector_PushBack<LargePayload, 100'000>);

} // namespace mcds::memory::bench
