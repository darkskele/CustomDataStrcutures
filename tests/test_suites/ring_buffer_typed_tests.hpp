#pragma once

#include <gtest/gtest.h>
#include "test_types/custom_types.hpp"

#include <vector>
#include <algorithm>
#include <random>

namespace mcds::tests
{
    // Typed test fixture for ring buffer containers
    template <typename RingBufferType>
    class RingBufferTypedTest : public ::testing::Test
    {
    protected:
        using ValueType = typename RingBufferType::value_type;
        static constexpr size_t Capacity = RingBufferType::capacity_value;

        void SetUp() override
        {
            buffer = std::make_unique<RingBufferType>();
        }

        void TearDown() override
        {
            buffer.reset();

            // Check for leaks if using TrackedType
            if constexpr (std::is_same_v<ValueType, TrackedType>)
            {
                EXPECT_FALSE(has_leaks()) << "Memory leak detected: " << leaked_objects() << " objects";
                reset_tracking();
            }
        }

        // Helper to create values
        ValueType make_value(int value)
        {
            if constexpr (std::is_same_v<ValueType, int>)
            {
                return value;
            }
            else if constexpr (std::is_same_v<ValueType, double>)
            {
                return static_cast<double>(value);
            }
            else if constexpr (std::is_same_v<ValueType, ComplexType>)
            {
                return ComplexType(value, value * 2, "val_" + std::to_string(value));
            }
            else if constexpr (std::is_same_v<ValueType, TrackedType>)
            {
                return TrackedType(value);
            }
        }

        // Helper to verify value
        bool verify_value(const ValueType &val, int expected)
        {
            if constexpr (std::is_same_v<ValueType, int>)
            {
                return val == expected;
            }
            else if constexpr (std::is_same_v<ValueType, double>)
            {
                return val == static_cast<double>(expected);
            }
            else if constexpr (std::is_same_v<ValueType, ComplexType>)
            {
                return val.primary_id == expected;
            }
            else if constexpr (std::is_same_v<ValueType, TrackedType>)
            {
                return val.id == expected;
            }
        }

        std::unique_ptr<RingBufferType> buffer;
    };

} // namespace mcds::tests
