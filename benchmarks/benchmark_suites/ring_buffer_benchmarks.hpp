#pragma once

#include <benchmark/benchmark.h>
#include <vector>
#include <cstdint>
#include <random>

#include "data_types.hpp"

// Macro to define all benchmarks for a ring buffer
#define DEFINE_RING_BUFFER_BENCHMARKS(NAME, CONTAINER_TEMPLATE, CAPACITY)                    \
    namespace mcds::bench                                                                    \
    {                                                                                        \
        /* Push Back */                                                                      \
        template <typename Value>                                                            \
        static void BM_##NAME##_PushBack(benchmark::State &state)                            \
        {                                                                                    \
            const int N = static_cast<int>(state.range(0));                                  \
            std::vector<Value> payloads(N);                                                  \
            for (int i = 0; i < N; ++i)                                                      \
            {                                                                                \
                payloads[i] = make_payload<Value>(static_cast<std::uint64_t>(i));            \
            }                                                                                \
            for (auto _ : state)                                                             \
            {                                                                                \
                CONTAINER_TEMPLATE<Value, CAPACITY> container;                               \
                for (int i = 0; i < N; ++i)                                                  \
                {                                                                            \
                    container.push_back(payloads[i]);                                        \
                }                                                                            \
                benchmark::DoNotOptimize(container);                                         \
                benchmark::ClobberMemory();                                                  \
            }                                                                                \
        }                                                                                    \
                                                                                             \
        /* Push Front */                                                                     \
        template <typename Value>                                                            \
        static void BM_##NAME##_PushFront(benchmark::State &state)                           \
        {                                                                                    \
            const int N = static_cast<int>(state.range(0));                                  \
            std::vector<Value> payloads(N);                                                  \
            for (int i = 0; i < N; ++i)                                                      \
            {                                                                                \
                payloads[i] = make_payload<Value>(static_cast<std::uint64_t>(i));            \
            }                                                                                \
            for (auto _ : state)                                                             \
            {                                                                                \
                CONTAINER_TEMPLATE<Value, CAPACITY> container;                               \
                for (int i = 0; i < N; ++i)                                                  \
                {                                                                            \
                    container.push_front(payloads[i]);                                       \
                }                                                                            \
                benchmark::DoNotOptimize(container);                                         \
                benchmark::ClobberMemory();                                                  \
            }                                                                                \
        }                                                                                    \
                                                                                             \
        /* Pop Front */                                                                      \
        template <typename Value>                                                            \
        static void BM_##NAME##_PopFront(benchmark::State &state)                            \
        {                                                                                    \
            const int N = static_cast<int>(state.range(0));                                  \
            for (auto _ : state)                                                             \
            {                                                                                \
                state.PauseTiming();                                                         \
                CONTAINER_TEMPLATE<Value, CAPACITY> container;                               \
                for (int i = 0; i < N; ++i)                                                  \
                {                                                                            \
                    container.push_back(make_payload<Value>(static_cast<std::uint64_t>(i))); \
                }                                                                            \
                state.ResumeTiming();                                                        \
                for (int i = 0; i < N; ++i)                                                  \
                {                                                                            \
                    container.pop_front();                                                   \
                }                                                                            \
                benchmark::ClobberMemory();                                                  \
            }                                                                                \
        }                                                                                    \
                                                                                             \
        /* Pop Back */                                                                       \
        template <typename Value>                                                            \
        static void BM_##NAME##_PopBack(benchmark::State &state)                             \
        {                                                                                    \
            const int N = static_cast<int>(state.range(0));                                  \
            for (auto _ : state)                                                             \
            {                                                                                \
                state.PauseTiming();                                                         \
                CONTAINER_TEMPLATE<Value, CAPACITY> container;                               \
                for (int i = 0; i < N; ++i)                                                  \
                {                                                                            \
                    container.push_back(make_payload<Value>(static_cast<std::uint64_t>(i))); \
                }                                                                            \
                state.ResumeTiming();                                                        \
                for (int i = 0; i < N; ++i)                                                  \
                {                                                                            \
                    container.pop_back();                                                    \
                }                                                                            \
                benchmark::ClobberMemory();                                                  \
            }                                                                                \
        }                                                                                    \
                                                                                             \
        /* Random Access */                                                                  \
        template <typename Value>                                                            \
        static void BM_##NAME##_RandomAccess(benchmark::State &state)                        \
        {                                                                                    \
            const int N = static_cast<int>(state.range(0));                                  \
            CONTAINER_TEMPLATE<Value, CAPACITY> container;                                   \
            for (int i = 0; i < N; ++i)                                                      \
            {                                                                                \
                container.push_back(make_payload<Value>(static_cast<std::uint64_t>(i)));     \
            }                                                                                \
            std::vector<int> indices(N);                                                     \
            std::mt19937 rng(42);                                                            \
            std::uniform_int_distribution<int> dist(0, N - 1);                               \
            for (int i = 0; i < N; ++i)                                                      \
            {                                                                                \
                indices[i] = dist(rng);                                                      \
            }                                                                                \
            for (auto _ : state)                                                             \
            {                                                                                \
                for (int idx : indices)                                                      \
                {                                                                            \
                    auto &v = container[idx];                                                \
                    benchmark::DoNotOptimize(v);                                             \
                }                                                                            \
            }                                                                                \
        }                                                                                    \
                                                                                             \
        /* Front/Back Access */                                                              \
        template <typename Value>                                                            \
        static void BM_##NAME##_FrontBackAccess(benchmark::State &state)                     \
        {                                                                                    \
            const int N = static_cast<int>(state.range(0));                                  \
            CONTAINER_TEMPLATE<Value, CAPACITY> container;                                   \
            for (int i = 0; i < 2; ++i)                                                      \
            {                                                                                \
                container.push_back(make_payload<Value>(static_cast<std::uint64_t>(i)));     \
            }                                                                                \
            for (auto _ : state)                                                             \
            {                                                                                \
                for (int i = 0; i < N; ++i)                                                  \
                {                                                                            \
                    auto &f = container.front();                                             \
                    auto &b = container.back();                                              \
                    benchmark::DoNotOptimize(f);                                             \
                    benchmark::DoNotOptimize(b);                                             \
                }                                                                            \
            }                                                                                \
        }                                                                                    \
    }

// Macro to register all benchmarks for a ring buffer
#define REGISTER_RING_BUFFER_BENCHMARKS(NAME, PAYLOAD)       \
    BENCHMARK_TEMPLATE(BM_##NAME##_PushBack, PAYLOAD)        \
        ->Arg(1'000)                                         \
        ->Arg(10'000)                                        \
        ->Arg(100'000);                                      \
    BENCHMARK_TEMPLATE(BM_##NAME##_PushFront, PAYLOAD)       \
        ->Arg(1'000)                                         \
        ->Arg(10'000)                                        \
        ->Arg(100'000);                                      \
    BENCHMARK_TEMPLATE(BM_##NAME##_PopFront, PAYLOAD)        \
        ->Arg(1'000)                                         \
        ->Arg(10'000)                                        \
        ->Arg(100'000);                                      \
    BENCHMARK_TEMPLATE(BM_##NAME##_PopBack, PAYLOAD)         \
        ->Arg(1'000)                                         \
        ->Arg(10'000)                                        \
        ->Arg(100'000);                                      \
    BENCHMARK_TEMPLATE(BM_##NAME##_RandomAccess, PAYLOAD)    \
        ->Arg(1'000)                                         \
        ->Arg(10'000)                                        \
        ->Arg(100'000);                                      \
    BENCHMARK_TEMPLATE(BM_##NAME##_FrontBackAccess, PAYLOAD) \
        ->Arg(1'000)                                         \
        ->Arg(10'000)                                        \
        ->Arg(100'000);
