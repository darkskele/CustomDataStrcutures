#pragma once

#include <benchmark/benchmark.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <random>
#include <type_traits>
#include <vector>

#include "data_types.hpp"

// Macro to define all benchmarks for a container
#define DEFINE_ASSOCIATIVE_CONTAINER_BENCHMARKS(NAME, CONTAINER_TEMPLATE)                    \
    namespace mcds::bench                                                                    \
    {                                                                                        \
        /* Insert Sequential */                                                              \
        template <typename Value>                                                            \
        static void BM_##NAME##_InsertSequential(benchmark::State &state)                    \
        {                                                                                    \
            const int N = static_cast<int>(state.range(0));                                  \
            std::vector<Value> payloads(N);                                                  \
            for (int i = N - 1; i >= 0; --i)                                                 \
            {                                                                                \
                payloads[i] = make_payload<Value>(static_cast<std::uint64_t>(i));            \
            }                                                                                \
            for (auto _ : state)                                                             \
            {                                                                                \
                CONTAINER_TEMPLATE<int, Value> container;                                    \
                for (int i = N - 1; i >= 0; --i)                                             \
                {                                                                            \
                    container.insert(i, payloads[i]);                                        \
                }                                                                            \
                benchmark::DoNotOptimize(container);                                         \
                benchmark::ClobberMemory();                                                  \
            }                                                                                \
        }                                                                                    \
                                                                                             \
        /* Find Hit */                                                                       \
        template <typename Value>                                                            \
        static void BM_##NAME##_FindHit(benchmark::State &state)                             \
        {                                                                                    \
            const int N = static_cast<int>(state.range(0));                                  \
            CONTAINER_TEMPLATE<int, Value> container;                                        \
            for (int i = 0; i < N; ++i)                                                      \
            {                                                                                \
                container.insert(i, make_payload<Value>(static_cast<std::uint64_t>(i)));     \
            }                                                                                \
            std::vector<int> queries(N);                                                     \
            std::mt19937 rng(123);                                                           \
            std::uniform_int_distribution<int> dist(0, N - 1);                               \
            for (int i = 0; i < N; ++i)                                                      \
            {                                                                                \
                queries[i] = dist(rng);                                                      \
            }                                                                                \
            for (auto _ : state)                                                             \
            {                                                                                \
                for (int k : queries)                                                        \
                {                                                                            \
                    auto *v = container.find(k);                                             \
                    benchmark::DoNotOptimize(v);                                             \
                }                                                                            \
            }                                                                                \
        }                                                                                    \
                                                                                             \
        /* Find Miss */                                                                      \
        template <typename Value>                                                            \
        static void BM_##NAME##_FindMiss(benchmark::State &state)                            \
        {                                                                                    \
            const int N = static_cast<int>(state.range(0));                                  \
            CONTAINER_TEMPLATE<int, Value> container;                                        \
            for (int i = 0; i < N; ++i)                                                      \
            {                                                                                \
                container.insert(i * 2, make_payload<Value>(static_cast<std::uint64_t>(i))); \
            }                                                                                \
            std::vector<int> queries(N);                                                     \
            for (int i = 0; i < N; ++i)                                                      \
            {                                                                                \
                queries[i] = i * 2 + 1;                                                      \
            }                                                                                \
            for (auto _ : state)                                                             \
            {                                                                                \
                for (int k : queries)                                                        \
                {                                                                            \
                    auto *v = container.find(k);                                             \
                    benchmark::DoNotOptimize(v);                                             \
                }                                                                            \
            }                                                                                \
        }                                                                                    \
                                                                                             \
        /* Erase Sequential */                                                               \
        template <typename Value>                                                            \
        static void BM_##NAME##_EraseSequential(benchmark::State &state)                     \
        {                                                                                    \
            const int N = static_cast<int>(state.range(0));                                  \
            for (auto _ : state)                                                             \
            {                                                                                \
                state.PauseTiming();                                                         \
                CONTAINER_TEMPLATE<int, Value> container;                                    \
                for (int i = 0; i < N; ++i)                                                  \
                {                                                                            \
                    container.insert(i, make_payload<Value>(static_cast<std::uint64_t>(i))); \
                }                                                                            \
                state.ResumeTiming();                                                        \
                for (int i = 0; i < N; ++i)                                                  \
                {                                                                            \
                    bool ok = container.erase(i);                                            \
                    benchmark::DoNotOptimize(ok);                                            \
                }                                                                            \
                benchmark::ClobberMemory();                                                  \
            }                                                                                \
        }                                                                                    \
                                                                                             \
        /* Find Min/Max */                                                                   \
        template <typename Value>                                                            \
        static void BM_##NAME##_FindMinMax(benchmark::State &state)                          \
        {                                                                                    \
            const int N = static_cast<int>(state.range(0));                                  \
            CONTAINER_TEMPLATE<int, Value> container;                                        \
            for (int i = 0; i < N; ++i)                                                      \
            {                                                                                \
                container.insert(i, make_payload<Value>(static_cast<std::uint64_t>(i)));     \
            }                                                                                \
            for (auto _ : state)                                                             \
            {                                                                                \
                auto minv = container.find_min();                                            \
                auto maxv = container.find_max();                                            \
                benchmark::DoNotOptimize(minv);                                              \
                benchmark::DoNotOptimize(maxv);                                              \
            }                                                                                \
        }                                                                                    \
                                                                                             \
        /* Lower/Upper Bound */                                                              \
        template <typename Value>                                                            \
        static void BM_##NAME##_LowerUpperBound(benchmark::State &state)                     \
        {                                                                                    \
            const int N = static_cast<int>(state.range(0));                                  \
            CONTAINER_TEMPLATE<int, Value> container;                                        \
            for (int i = 0; i < N; ++i)                                                      \
            {                                                                                \
                container.insert(i * 2, make_payload<Value>(static_cast<std::uint64_t>(i))); \
            }                                                                                \
            std::vector<int> queries(N);                                                     \
            std::mt19937 rng(777);                                                           \
            std::uniform_int_distribution<int> dist(0, 2 * N);                               \
            for (int i = 0; i < N; ++i)                                                      \
            {                                                                                \
                queries[i] = dist(rng);                                                      \
            }                                                                                \
            for (auto _ : state)                                                             \
            {                                                                                \
                for (int q : queries)                                                        \
                {                                                                            \
                    auto *lb = container.lower_bound(q);                                     \
                    auto *ub = container.upper_bound(q);                                     \
                    benchmark::DoNotOptimize(lb);                                            \
                    benchmark::DoNotOptimize(ub);                                            \
                }                                                                            \
            }                                                                                \
        }                                                                                    \
                                                                                             \
        /* Operator[] */                                                                     \
        template <typename Value>                                                            \
        static void BM_##NAME##_OperatorBracket(benchmark::State &state)                     \
        {                                                                                    \
            const int N = static_cast<int>(state.range(0));                                  \
            std::vector<Value> payloads1(N), payloads2(N);                                   \
            for (int i = 0; i < N; ++i)                                                      \
            {                                                                                \
                payloads1[i] = make_payload<Value>(static_cast<std::uint64_t>(i));           \
                payloads2[i] = make_payload<Value>(static_cast<std::uint64_t>(i + 1));       \
            }                                                                                \
            for (auto _ : state)                                                             \
            {                                                                                \
                CONTAINER_TEMPLATE<int, Value> container;                                    \
                for (int i = 0; i < N; ++i)                                                  \
                {                                                                            \
                    container[i] = payloads1[i];                                             \
                }                                                                            \
                for (int i = 0; i < N; ++i)                                                  \
                {                                                                            \
                    container[i] = payloads2[i];                                             \
                }                                                                            \
                benchmark::DoNotOptimize(container);                                         \
                benchmark::ClobberMemory();                                                  \
            }                                                                                \
        }                                                                                    \
    }

// Macro to register all benchmarks for a container
#define REGISTER_ASSOCIATIVE_CONTAINER_BENCHMARKS(NAME, PAYLOAD) \
    BENCHMARK_TEMPLATE(BM_##NAME##_InsertSequential, PAYLOAD)    \
        ->Arg(1'000)                                             \
        ->Arg(10'000)                                            \
        ->Arg(100'000);                                          \
    BENCHMARK_TEMPLATE(BM_##NAME##_FindHit, PAYLOAD)             \
        ->Arg(1'000)                                             \
        ->Arg(10'000)                                            \
        ->Arg(100'000);                                          \
    BENCHMARK_TEMPLATE(BM_##NAME##_FindMiss, PAYLOAD)            \
        ->Arg(1'000)                                             \
        ->Arg(10'000)                                            \
        ->Arg(100'000);                                          \
    BENCHMARK_TEMPLATE(BM_##NAME##_EraseSequential, PAYLOAD)     \
        ->Arg(1'000)                                             \
        ->Arg(10'000)                                            \
        ->Arg(100'000);                                          \
    BENCHMARK_TEMPLATE(BM_##NAME##_FindMinMax, PAYLOAD)          \
        ->Arg(1'000)                                             \
        ->Arg(10'000)                                            \
        ->Arg(100'000);                                          \
    BENCHMARK_TEMPLATE(BM_##NAME##_LowerUpperBound, PAYLOAD)     \
        ->Arg(1'000)                                             \
        ->Arg(10'000)                                            \
        ->Arg(100'000);                                          \
    BENCHMARK_TEMPLATE(BM_##NAME##_OperatorBracket, PAYLOAD)     \
        ->Arg(1'000)                                             \
        ->Arg(10'000)                                            \
        ->Arg(100'000);
