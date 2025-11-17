#include <benchmark/benchmark.h>

#include "mcds/b_tree.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>
#include <type_traits>
#include <vector>

namespace mcds::bench
{

    // Payload types to stress different value sizes
    using SmallPayload = int;

    struct MediumPayload
    {
        std::array<std::uint64_t, 8> data; // 64 bytes
    };

    struct LargePayload
    {
        std::array<std::uint64_t, 64> data; // 512 bytes
    };

    template <typename T>
    inline T make_payload(std::uint64_t seed)
    {
        T v{};
        if constexpr (std::is_same_v<T, SmallPayload>)
        {
            v = static_cast<int>(seed);
        }
        else
        {
            for (size_t i = 0; i < v.data.size(); ++i)
            {
                v.data[i] = seed + static_cast<std::uint64_t>(i);
            }
        }
        return v;
    }

    // Insert benchmarks
    template <typename Value>
    static void BM_InsertSequential(benchmark::State &state)
    {
        const int N = static_cast<int>(state.range(0));

        // Pre-generate payloads once, outside timed loop
        std::vector<Value> payloads(N);
        for (int i = 0; i < N; ++i)
        {
            payloads[i] = make_payload<Value>(static_cast<std::uint64_t>(i));
        }

        for (auto _ : state)
        {
            BTree<int, Value> tree;
            for (int i = 0; i < N; ++i)
            {
                tree.insert(i, payloads[i]);
            }
            benchmark::DoNotOptimize(tree);
            benchmark::ClobberMemory();
        }
    }

    // Find benchmarks
    template <typename Value>
    static void BM_FindHit(benchmark::State &state)
    {
        const int N = static_cast<int>(state.range(0));

        BTree<int, Value> tree;
        for (int i = 0; i < N; ++i)
        {
            tree.insert(i, make_payload<Value>(static_cast<std::uint64_t>(i)));
        }

        std::vector<int> queries(N);
        std::mt19937 rng(123);
        std::uniform_int_distribution<int> dist(0, N - 1);
        for (int i = 0; i < N; ++i)
        {
            queries[i] = dist(rng);
        }

        for (auto _ : state)
        {
            for (int k : queries)
            {
                auto *v = tree.find(k);
                benchmark::DoNotOptimize(v);
            }
        }
    }

    template <typename Value>
    static void BM_FindMiss(benchmark::State &state)
    {
        const int N = static_cast<int>(state.range(0));

        BTree<int, Value> tree;
        for (int i = 0; i < N; ++i)
        {
            tree.insert(i * 2, make_payload<Value>(static_cast<std::uint64_t>(i)));
        }

        // Guaranteed misses
        std::vector<int> queries(N);
        for (int i = 0; i < N; ++i)
        {
            queries[i] = i * 2 + 1;
        }

        for (auto _ : state)
        {
            for (int k : queries)
            {
                auto *v = tree.find(k);
                benchmark::DoNotOptimize(v);
            }
        }
    }

    // Erase benchmark – build tree each time
    template <typename Value>
    static void BM_EraseSequential(benchmark::State &state)
    {
        const int N = static_cast<int>(state.range(0));

        for (auto _ : state)
        {
            state.PauseTiming();
            BTree<int, Value> tree;
            for (int i = 0; i < N; ++i)
            {
                tree.insert(i, make_payload<Value>(static_cast<std::uint64_t>(i)));
            }
            state.ResumeTiming();

            for (int i = 0; i < N; ++i)
            {
                bool ok = tree.erase(i);
                benchmark::DoNotOptimize(ok);
            }
            benchmark::ClobberMemory();
        }
    }

    // Min / Max
    template <typename Value>
    static void BM_FindMinMax(benchmark::State &state)
    {
        const int N = static_cast<int>(state.range(0));

        BTree<int, Value> tree;
        for (int i = 0; i < N; ++i)
        {
            tree.insert(i, make_payload<Value>(static_cast<std::uint64_t>(i)));
        }

        for (auto _ : state)
        {
            auto minv = tree.find_min();
            auto maxv = tree.find_max();
            benchmark::DoNotOptimize(minv);
            benchmark::DoNotOptimize(maxv);
        }
    }

    // lower_bound / upper_bound
    template <typename Value>
    static void BM_LowerUpperBound(benchmark::State &state)
    {
        const int N = static_cast<int>(state.range(0));

        BTree<int, Value> tree;
        for (int i = 0; i < N; ++i)
        {
            tree.insert(i * 2, make_payload<Value>(static_cast<std::uint64_t>(i)));
        }

        std::vector<int> queries(N);
        std::mt19937 rng(777);
        std::uniform_int_distribution<int> dist(0, 2 * N);
        for (int i = 0; i < N; ++i)
        {
            queries[i] = dist(rng);
        }

        for (auto _ : state)
        {
            for (int q : queries)
            {
                auto *lb = tree.lower_bound(q);
                auto *ub = tree.upper_bound(q);
                benchmark::DoNotOptimize(lb);
                benchmark::DoNotOptimize(ub);
            }
        }
    }

    // operator[] – we time both insert via [] and overwrite via []
    template <typename Value>
    static void BM_OperatorBracket(benchmark::State &state)
    {
        const int N = static_cast<int>(state.range(0));

        // Pre gen payloads
        std::vector<Value> payloads1(N), payloads2(N);
        for (int i = 0; i < N; ++i)
        {
            payloads1[i] = make_payload<Value>(static_cast<std::uint64_t>(i));
            payloads2[i] = make_payload<Value>(static_cast<std::uint64_t>(i + 1));
        }

        for (auto _ : state)
        {
            BTree<int, Value> tree;
            // inserts
            for (int i = 0; i < N; ++i)
            {
                tree[i] = payloads1[i];
            }
            // overwrites
            for (int i = 0; i < N; ++i)
            {
                tree[i] = payloads2[i];
            }
            benchmark::DoNotOptimize(tree);
            benchmark::ClobberMemory();
        }
    }

    // Registration
#define BTREE_ARG_SIZES \
    ->Arg(1'000)        \
        ->Arg(10'000)   \
        ->Arg(100'000)

    // Small payload
    BENCHMARK_TEMPLATE(BM_InsertSequential, SmallPayload)
    BTREE_ARG_SIZES;
    BENCHMARK_TEMPLATE(BM_FindHit, SmallPayload)
    BTREE_ARG_SIZES;
    BENCHMARK_TEMPLATE(BM_FindMiss, SmallPayload)
    BTREE_ARG_SIZES;
    BENCHMARK_TEMPLATE(BM_EraseSequential, SmallPayload)
    BTREE_ARG_SIZES;
    BENCHMARK_TEMPLATE(BM_FindMinMax, SmallPayload)
    BTREE_ARG_SIZES;
    BENCHMARK_TEMPLATE(BM_LowerUpperBound, SmallPayload)
    BTREE_ARG_SIZES;
    BENCHMARK_TEMPLATE(BM_OperatorBracket, SmallPayload)
    BTREE_ARG_SIZES;

    // Medium payload (~64B)
    BENCHMARK_TEMPLATE(BM_InsertSequential, MediumPayload)
    BTREE_ARG_SIZES;
    BENCHMARK_TEMPLATE(BM_FindHit, MediumPayload)
    BTREE_ARG_SIZES;
    BENCHMARK_TEMPLATE(BM_FindMiss, MediumPayload)
    BTREE_ARG_SIZES;
    BENCHMARK_TEMPLATE(BM_EraseSequential, MediumPayload)
    BTREE_ARG_SIZES;
    BENCHMARK_TEMPLATE(BM_FindMinMax, MediumPayload)
    BTREE_ARG_SIZES;
    BENCHMARK_TEMPLATE(BM_LowerUpperBound, MediumPayload)
    BTREE_ARG_SIZES;
    BENCHMARK_TEMPLATE(BM_OperatorBracket, MediumPayload)
    BTREE_ARG_SIZES;

    // Large payload (~512B)
    BENCHMARK_TEMPLATE(BM_InsertSequential, LargePayload)
    BTREE_ARG_SIZES;
    BENCHMARK_TEMPLATE(BM_FindHit, LargePayload)
    BTREE_ARG_SIZES;
    BENCHMARK_TEMPLATE(BM_FindMiss, LargePayload)
    BTREE_ARG_SIZES;
    BENCHMARK_TEMPLATE(BM_EraseSequential, LargePayload)
    BTREE_ARG_SIZES;
    BENCHMARK_TEMPLATE(BM_FindMinMax, LargePayload)
    BTREE_ARG_SIZES;
    BENCHMARK_TEMPLATE(BM_LowerUpperBound, LargePayload)
    BTREE_ARG_SIZES;
    BENCHMARK_TEMPLATE(BM_OperatorBracket, LargePayload)
    BTREE_ARG_SIZES;

    BENCHMARK_MAIN();

} // namespace mcds::bench
