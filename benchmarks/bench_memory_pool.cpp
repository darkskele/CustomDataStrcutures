#include <benchmark/benchmark.h>
#include <vector>
#include <random>
#include <algorithm>

#include "mcds/memory/memory_pool.hpp"
#include "benchmark_suites/data_types.hpp"

namespace mcds::bench
{

    using namespace memory;

    template <typename PayloadType, size_t CAPACITY>
    static void BM_MemoryPool_Allocate_Single(benchmark::State &state)
    {
        memory_pool<PayloadType, CAPACITY> pool;
        uint64_t seed = 0;

        for (auto _ : state)
        {
            auto payload = make_payload<PayloadType>(seed++);
            auto idx = pool.allocate(payload);
            benchmark::DoNotOptimize(idx);
            benchmark::ClobberMemory();
            pool.deallocate(idx);
        }

        state.SetItemsProcessed(state.iterations());
        state.SetLabel(std::to_string(sizeof(PayloadType)) + "B");
    }

    template <typename PayloadType, size_t CAPACITY>
    static void BM_MemoryPool_AllocateOnly(benchmark::State &state)
    {
        memory_pool<PayloadType, CAPACITY> pool;
        uint64_t seed = 0;
        std::vector<typename decltype(pool)::index_type> allocated_indices;
        allocated_indices.reserve(pool.capacity());

        for (auto _ : state)
        {
            state.PauseTiming();
            // Reset pool by deallocating everything
            for (auto idx : allocated_indices)
            {
                pool.deallocate(idx);
            }
            allocated_indices.clear();
            state.ResumeTiming();

            // Allocate as many as possible
            while (pool.has_free())
            {
                auto payload = make_payload<PayloadType>(seed++);
                auto idx = pool.allocate(payload);
                benchmark::DoNotOptimize(idx);
                allocated_indices.push_back(idx);
            }
            benchmark::ClobberMemory();
        }

        state.SetItemsProcessed(state.iterations() * pool.capacity());
        state.SetLabel(std::to_string(sizeof(PayloadType)) + "B");
    }

    template <typename PayloadType, size_t CAPACITY>
    static void BM_MemoryPool_DeallocateOnly(benchmark::State &state)
    {
        memory_pool<PayloadType, CAPACITY> pool;
        std::vector<typename decltype(pool)::index_type> indices;
        indices.reserve(pool.capacity());

        for (auto _ : state)
        {
            state.PauseTiming();
            // Fill pool
            indices.clear();
            for (uint64_t i = 0; i < pool.capacity(); ++i)
            {
                auto payload = make_payload<PayloadType>(i);
                indices.push_back(pool.allocate(payload));
            }
            state.ResumeTiming();

            // deallocate all
            for (auto idx : indices)
            {
                pool.deallocate(idx);
                benchmark::ClobberMemory();
            }
        }

        state.SetItemsProcessed(state.iterations() * pool.capacity());
        state.SetLabel(std::to_string(sizeof(PayloadType)) + "B");
    }

    template <typename PayloadType, size_t CAPACITY>
    static void BM_MemoryPool_RandomAccess(benchmark::State &state)
    {
        memory_pool<PayloadType, CAPACITY> pool;
        const size_t num_objects = pool.capacity();

        std::vector<typename decltype(pool)::index_type> indices;
        indices.reserve(num_objects);

        // Pre allocate objects
        for (size_t i = 0; i < num_objects; ++i)
        {
            auto payload = make_payload<PayloadType>(static_cast<int>(i));
            indices.push_back(pool.allocate(payload));
        }

        // Shuffle access order once
        std::mt19937 gen(42);
        std::shuffle(indices.begin(), indices.end(), gen);

        for (auto _ : state)
        {
            // Walk the pool in random order
            for (auto idx : indices)
            {
                auto &obj = pool[idx];
                benchmark::DoNotOptimize(obj);
            }
            benchmark::ClobberMemory();
        }

        state.SetItemsProcessed(state.iterations() * num_objects);
        state.SetLabel(std::to_string(sizeof(PayloadType)) +
                       "B, objects=" + std::to_string(num_objects));
    }

    template <typename PayloadType, size_t CAPACITY>
    static void BM_MemoryPool_SequentialAllocation(benchmark::State &state)
    {
        for (auto _ : state)
        {
            // Setup
            state.PauseTiming();
            memory_pool<PayloadType, CAPACITY> pool;
            state.ResumeTiming();

            // Fill pool sequentially
            for (uint64_t i = 0; i < pool.capacity(); ++i)
            {
                auto idx = pool.allocate(make_payload<PayloadType>(i));
                benchmark::DoNotOptimize(idx);
            }
            benchmark::ClobberMemory();
        }

        state.SetItemsProcessed(state.iterations() * CAPACITY);
        state.SetLabel(std::to_string(sizeof(PayloadType)) + "B");
    }

    template <typename PayloadType, size_t CAPACITY>
    static void BM_MemoryPool_SequentialDeallocation(benchmark::State &state)
    {
        memory_pool<PayloadType, CAPACITY> pool;
        std::vector<typename decltype(pool)::index_type> indices;
        indices.reserve(pool.capacity());

        for (auto _ : state)
        {
            state.PauseTiming();
            // Fill pool sequentially
            indices.clear();
            for (uint64_t i = 0; i < pool.capacity(); ++i)
            {
                auto payload = make_payload<PayloadType>(i);
                indices.push_back(pool.allocate(payload));
            }
            state.ResumeTiming();

            // Deallocate sequentially
            for (auto idx : indices)
            {
                pool.deallocate(idx);
            }
            benchmark::ClobberMemory();
        }

        state.SetItemsProcessed(state.iterations() * pool.capacity());
        state.SetLabel(std::to_string(sizeof(PayloadType)) + "B");
    }

    template <typename PayloadType, size_t CAPACITY>
    static void BM_MemoryPool_RandomDeallocation(benchmark::State &state)
    {
        memory_pool<PayloadType, CAPACITY> pool;
        std::vector<typename decltype(pool)::index_type> indices;
        indices.reserve(pool.capacity());

        for (auto _ : state)
        {
            state.PauseTiming();
            // Fill pool
            indices.clear();
            for (uint64_t i = 0; i < pool.capacity(); ++i)
            {
                auto payload = make_payload<PayloadType>(i);
                indices.push_back(pool.allocate(payload));
            }

            // Shuffle for random deallocation
            std::mt19937 gen(42);
            std::shuffle(indices.begin(), indices.end(), gen);
            state.ResumeTiming();

            // Deallocate randomly
            for (auto idx : indices)
            {
                pool.deallocate(idx);
            }
            benchmark::ClobberMemory();
        }

        state.SetItemsProcessed(state.iterations() * pool.capacity());
        state.SetLabel(std::to_string(sizeof(PayloadType)) + "B");
    }

    template <typename PayloadType, size_t CAPACITY>
    static void BM_MemoryPool_FragmentationPattern(benchmark::State &state)
    {
        memory_pool<PayloadType, CAPACITY> pool;
        std::vector<typename decltype(pool)::index_type> active_indices;
        const size_t batch_size = pool.capacity() / 10; // 10% batches
        active_indices.reserve(pool.capacity());

        for (auto _ : state)
        {
            state.PauseTiming();
            // Clear pool
            for (auto idx : active_indices)
            {
                pool.deallocate(idx);
            }
            active_indices.clear();
            state.ResumeTiming();

            // Simulate order book churn: allocate batch, deallocate half, repeat
            for (size_t cycle = 0; cycle < 10; ++cycle)
            {
                // Allocate batch
                for (size_t i = 0; i < batch_size; ++i)
                {
                    if (pool.has_free())
                    {
                        auto idx = pool.allocate(make_payload<PayloadType>(cycle * 100 + i));
                        benchmark::DoNotOptimize(idx);
                        active_indices.push_back(idx);
                    }
                }

                // Deallocate half (FIFO pattern)
                size_t to_remove = active_indices.size() / 2;
                for (size_t i = 0; i < to_remove; ++i)
                {
                    pool.deallocate(active_indices[i]);
                }
                active_indices.erase(active_indices.begin(), active_indices.begin() + to_remove);
                benchmark::ClobberMemory();
            }
        }

        state.SetItemsProcessed(state.iterations() * 10 * batch_size);
        state.SetLabel(std::to_string(sizeof(PayloadType)) + "B");
    }

    template <typename PayloadType, size_t CAPACITY>
    static void BM_MemoryPool_SlabExhaustion(benchmark::State &state)
    {
        memory_pool<PayloadType, CAPACITY> pool;
        const size_t slab_capacity = pool.capacity() / 8; // Approximate single slab capacity
        std::vector<typename decltype(pool)::index_type> indices;
        indices.reserve(slab_capacity * 2);

        for (auto _ : state)
        {
            state.PauseTiming();
            // Clear pool
            for (auto idx : indices)
            {
                pool.deallocate(idx);
            }
            indices.clear();
            state.ResumeTiming();

            // Allocate enough to potentially exhaust first slab and move to second
            for (size_t i = 0; i < slab_capacity * 2 && pool.has_free(); ++i)
            {
                auto idx = pool.allocate(make_payload<PayloadType>(i));
                benchmark::DoNotOptimize(idx);
                indices.push_back(idx);
            }
            benchmark::ClobberMemory();
        }

        state.SetItemsProcessed(state.iterations() * indices.size());
        state.SetLabel(std::to_string(sizeof(PayloadType)) + "B, slab_switch");
    }

    template <typename PayloadType, size_t CAPACITY>
    static void BM_MemoryPool_CrossSlabAccess(benchmark::State &state)
    {
        memory_pool<PayloadType, CAPACITY> pool;
        std::vector<typename decltype(pool)::index_type> indices;

        // Allocate across all slabs
        while (pool.has_free())
        {
            indices.push_back(pool.allocate(make_payload<PayloadType>(indices.size())));
        }

        // Shuffle to ensure cross-slab access pattern
        std::mt19937 gen(42);
        std::shuffle(indices.begin(), indices.end(), gen);

        for (auto _ : state)
        {
            // Access pattern that jumps between slabs
            for (auto idx : indices)
            {
                auto &obj = pool[idx];
                benchmark::DoNotOptimize(obj);
            }
            benchmark::ClobberMemory();
        }

        state.SetItemsProcessed(state.iterations() * indices.size());
        state.SetLabel(std::to_string(sizeof(PayloadType)) + "B, cross_slab");
    }

    // Small Payload
    BENCHMARK(BM_MemoryPool_Allocate_Single<SmallPayload, 1'000>);
    BENCHMARK(BM_MemoryPool_Allocate_Single<SmallPayload, 10'000>);
    BENCHMARK(BM_MemoryPool_Allocate_Single<SmallPayload, 100'000>);

    BENCHMARK(BM_MemoryPool_AllocateOnly<SmallPayload, 1'000>);
    BENCHMARK(BM_MemoryPool_AllocateOnly<SmallPayload, 10'000>);
    BENCHMARK(BM_MemoryPool_AllocateOnly<SmallPayload, 100'000>);

    BENCHMARK(BM_MemoryPool_DeallocateOnly<SmallPayload, 1'000>);
    BENCHMARK(BM_MemoryPool_DeallocateOnly<SmallPayload, 10'000>);
    BENCHMARK(BM_MemoryPool_DeallocateOnly<SmallPayload, 100'000>);

    BENCHMARK(BM_MemoryPool_RandomAccess<SmallPayload, 1'000>);
    BENCHMARK(BM_MemoryPool_RandomAccess<SmallPayload, 10'000>);
    BENCHMARK(BM_MemoryPool_RandomAccess<SmallPayload, 100'000>);

    BENCHMARK(BM_MemoryPool_SequentialAllocation<SmallPayload, 1'000>);
    BENCHMARK(BM_MemoryPool_SequentialAllocation<SmallPayload, 10'000>);
    BENCHMARK(BM_MemoryPool_SequentialAllocation<SmallPayload, 100'000>);

    BENCHMARK(BM_MemoryPool_SequentialDeallocation<SmallPayload, 1'000>);
    BENCHMARK(BM_MemoryPool_SequentialDeallocation<SmallPayload, 10'000>);
    BENCHMARK(BM_MemoryPool_SequentialDeallocation<SmallPayload, 100'000>);

    BENCHMARK(BM_MemoryPool_RandomDeallocation<SmallPayload, 1'000>);
    BENCHMARK(BM_MemoryPool_RandomDeallocation<SmallPayload, 10'000>);
    BENCHMARK(BM_MemoryPool_RandomDeallocation<SmallPayload, 100'000>);

    BENCHMARK(BM_MemoryPool_FragmentationPattern<SmallPayload, 1'000>);
    BENCHMARK(BM_MemoryPool_FragmentationPattern<SmallPayload, 10'000>);
    BENCHMARK(BM_MemoryPool_FragmentationPattern<SmallPayload, 100'000>);

    BENCHMARK(BM_MemoryPool_SlabExhaustion<SmallPayload, 1'000>);
    BENCHMARK(BM_MemoryPool_SlabExhaustion<SmallPayload, 10'000>);
    BENCHMARK(BM_MemoryPool_SlabExhaustion<SmallPayload, 100'000>);

    BENCHMARK(BM_MemoryPool_CrossSlabAccess<SmallPayload, 1'000>);
    BENCHMARK(BM_MemoryPool_CrossSlabAccess<SmallPayload, 10'000>);
    BENCHMARK(BM_MemoryPool_CrossSlabAccess<SmallPayload, 100'000>);

    // Medium Payload
    BENCHMARK(BM_MemoryPool_Allocate_Single<MediumPayload, 1'000>);
    BENCHMARK(BM_MemoryPool_Allocate_Single<MediumPayload, 10'000>);
    BENCHMARK(BM_MemoryPool_Allocate_Single<MediumPayload, 100'000>);

    BENCHMARK(BM_MemoryPool_AllocateOnly<MediumPayload, 1'000>);
    BENCHMARK(BM_MemoryPool_AllocateOnly<MediumPayload, 10'000>);
    BENCHMARK(BM_MemoryPool_AllocateOnly<MediumPayload, 100'000>);

    BENCHMARK(BM_MemoryPool_DeallocateOnly<MediumPayload, 1'000>);
    BENCHMARK(BM_MemoryPool_DeallocateOnly<MediumPayload, 10'000>);
    BENCHMARK(BM_MemoryPool_DeallocateOnly<MediumPayload, 100'000>);

    BENCHMARK(BM_MemoryPool_RandomAccess<MediumPayload, 1'000>);
    BENCHMARK(BM_MemoryPool_RandomAccess<MediumPayload, 10'000>);
    BENCHMARK(BM_MemoryPool_RandomAccess<MediumPayload, 100'000>);

    BENCHMARK(BM_MemoryPool_SequentialAllocation<MediumPayload, 1'000>);
    BENCHMARK(BM_MemoryPool_SequentialAllocation<MediumPayload, 10'000>);
    BENCHMARK(BM_MemoryPool_SequentialAllocation<MediumPayload, 100'000>);

    BENCHMARK(BM_MemoryPool_SequentialDeallocation<MediumPayload, 1'000>);
    BENCHMARK(BM_MemoryPool_SequentialDeallocation<MediumPayload, 10'000>);
    BENCHMARK(BM_MemoryPool_SequentialDeallocation<MediumPayload, 100'000>);

    BENCHMARK(BM_MemoryPool_RandomDeallocation<MediumPayload, 1'000>);
    BENCHMARK(BM_MemoryPool_RandomDeallocation<MediumPayload, 10'000>);
    BENCHMARK(BM_MemoryPool_RandomDeallocation<MediumPayload, 100'000>);

    BENCHMARK(BM_MemoryPool_FragmentationPattern<MediumPayload, 1'000>);
    BENCHMARK(BM_MemoryPool_FragmentationPattern<MediumPayload, 10'000>);
    BENCHMARK(BM_MemoryPool_FragmentationPattern<MediumPayload, 100'000>);

    BENCHMARK(BM_MemoryPool_SlabExhaustion<MediumPayload, 1'000>);
    BENCHMARK(BM_MemoryPool_SlabExhaustion<MediumPayload, 10'000>);
    BENCHMARK(BM_MemoryPool_SlabExhaustion<MediumPayload, 100'000>);

    BENCHMARK(BM_MemoryPool_CrossSlabAccess<MediumPayload, 1'000>);
    BENCHMARK(BM_MemoryPool_CrossSlabAccess<MediumPayload, 10'000>);
    BENCHMARK(BM_MemoryPool_CrossSlabAccess<MediumPayload, 100'000>);

    // Large Payload
    BENCHMARK(BM_MemoryPool_Allocate_Single<LargePayload, 1'000>);
    BENCHMARK(BM_MemoryPool_Allocate_Single<LargePayload, 10'000>);
    BENCHMARK(BM_MemoryPool_Allocate_Single<LargePayload, 100'000>);

    BENCHMARK(BM_MemoryPool_AllocateOnly<LargePayload, 1'000>);
    BENCHMARK(BM_MemoryPool_AllocateOnly<LargePayload, 10'000>);
    BENCHMARK(BM_MemoryPool_AllocateOnly<LargePayload, 100'000>);

    BENCHMARK(BM_MemoryPool_DeallocateOnly<LargePayload, 1'000>);
    BENCHMARK(BM_MemoryPool_DeallocateOnly<LargePayload, 10'000>);
    BENCHMARK(BM_MemoryPool_DeallocateOnly<LargePayload, 100'000>);

    BENCHMARK(BM_MemoryPool_RandomAccess<LargePayload, 1'000>);
    BENCHMARK(BM_MemoryPool_RandomAccess<LargePayload, 10'000>);
    BENCHMARK(BM_MemoryPool_RandomAccess<LargePayload, 100'000>);

    BENCHMARK(BM_MemoryPool_SequentialAllocation<LargePayload, 1'000>);
    BENCHMARK(BM_MemoryPool_SequentialAllocation<LargePayload, 10'000>);
    BENCHMARK(BM_MemoryPool_SequentialAllocation<LargePayload, 100'000>);

    BENCHMARK(BM_MemoryPool_SequentialDeallocation<LargePayload, 1'000>);
    BENCHMARK(BM_MemoryPool_SequentialDeallocation<LargePayload, 10'000>);
    BENCHMARK(BM_MemoryPool_SequentialDeallocation<LargePayload, 100'000>);

    BENCHMARK(BM_MemoryPool_RandomDeallocation<LargePayload, 1'000>);
    BENCHMARK(BM_MemoryPool_RandomDeallocation<LargePayload, 10'000>);
    BENCHMARK(BM_MemoryPool_RandomDeallocation<LargePayload, 100'000>);

    BENCHMARK(BM_MemoryPool_FragmentationPattern<LargePayload, 1'000>);
    BENCHMARK(BM_MemoryPool_FragmentationPattern<LargePayload, 10'000>);
    BENCHMARK(BM_MemoryPool_FragmentationPattern<LargePayload, 100'000>);

    BENCHMARK(BM_MemoryPool_SlabExhaustion<LargePayload, 1'000>);
    BENCHMARK(BM_MemoryPool_SlabExhaustion<LargePayload, 10'000>);
    BENCHMARK(BM_MemoryPool_SlabExhaustion<LargePayload, 100'000>);

    BENCHMARK(BM_MemoryPool_CrossSlabAccess<LargePayload, 1'000>);
    BENCHMARK(BM_MemoryPool_CrossSlabAccess<LargePayload, 10'000>);
    BENCHMARK(BM_MemoryPool_CrossSlabAccess<LargePayload, 100'000>);

} // namespace mcds::bench
