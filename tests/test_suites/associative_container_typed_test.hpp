#pragma once

#include <gtest/gtest.h>

#include "test_types/custom_types.hpp"

#include <vector>
#include <algorithm>
#include <random>
#include <set>

namespace mcds::tests
{

    // Typed test fixture
    template <typename ContainerType>
    class AssociativeContainerTypedTest : public ::testing::Test
    {
    protected:
        using KeyType = typename ContainerType::key_type;
        using ValueType = typename ContainerType::value_type;

        void SetUp() override
        {
            container = std::make_unique<ContainerType>();
        }

        void TearDown() override
        {
            container.reset();

            // Check for leaks if using TrackedType
            if constexpr (std::is_same_v<KeyType, TrackedType> ||
                          std::is_same_v<ValueType, TrackedType>)
            {
                EXPECT_FALSE(has_leaks())
                    << "Memory leak detected: " << leaked_objects() << " objects";
                reset_tracking();
            }
        }

        // Helper to create keys
        KeyType make_key(int value)
        {
            if constexpr (std::is_same_v<KeyType, int>)
            {
                return value;
            }
            else if constexpr (std::is_same_v<KeyType, double>)
            {
                return static_cast<double>(value);
            }
            else if constexpr (std::is_same_v<KeyType, ComplexType>)
            {
                return ComplexType(value, value * 2, "key_" + std::to_string(value));
            }
            else if constexpr (std::is_same_v<KeyType, TrackedType>)
            {
                return TrackedType(value);
            }
        }

        // Helper to create values
        ValueType make_value(int value)
        {
            if constexpr (std::is_same_v<ValueType, int>)
            {
                return value * 100;
            }
            else if constexpr (std::is_same_v<ValueType, double>)
            {
                return static_cast<double>(value * 100);
            }
            else if constexpr (std::is_same_v<ValueType, ComplexType>)
            {
                return ComplexType(value * 100, value * 200, "val_" + std::to_string(value));
            }
            else if constexpr (std::is_same_v<ValueType, TrackedType>)
            {
                return TrackedType(value * 100);
            }
        }

        // Helper to verify value
        bool verify_value(const ValueType &val, int expected)
        {
            if constexpr (std::is_same_v<ValueType, int>)
            {
                return val == expected * 100;
            }
            else if constexpr (std::is_same_v<ValueType, double>)
            {
                return val == static_cast<double>(expected * 100);
            }
            else if constexpr (std::is_same_v<ValueType, ComplexType>)
            {
                return val.primary_id == expected * 100;
            }
            else if constexpr (std::is_same_v<ValueType, TrackedType>)
            {
                return val.id == expected * 100;
            }
        }

        // Helper to verify key
        bool verify_key(const KeyType &val, int expected)
        {
            if constexpr (std::is_same_v<KeyType, int>)
            {
                return val == expected;
            }
            else if constexpr (std::is_same_v<KeyType, double>)
            {
                return val == static_cast<double>(expected);
            }
            else if constexpr (std::is_same_v<KeyType, ComplexType>)
            {
                return val.primary_id == expected;
            }
            else if constexpr (std::is_same_v<KeyType, TrackedType>)
            {
                return val.id == expected;
            }
        }

        std::unique_ptr<ContainerType> container;
    };
}
