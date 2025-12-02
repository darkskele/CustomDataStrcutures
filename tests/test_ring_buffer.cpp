#include <gtest/gtest.h>

#include "mcds/ring_buffer.hpp"
#include "test_types/custom_types.hpp"
#include "test_suites/ring_buffer_typed_tests.hpp"

namespace mcds::tests
{
    // Helper trait to expose capacity as a static member
    template <typename T, size_t Cap, bool ForceHeap, size_t StackBudget>
    struct ring_buffer_with_capacity : public ring_buffer<T, Cap, ForceHeap, StackBudget>
    {
        using value_type = T;
        static constexpr size_t capacity_value = Cap;
    };

    // Create RingBuffer-specific derived suite
    template <typename RingBuffer>
    class RingBufferTest : public RingBufferTypedTest<RingBuffer>
    {
    protected:
        using RingBufferType = RingBuffer;
    };

    // Define concrete ring buffer type combinations with various capacities
    // Small capacity (power of 2)
    using RingBufferInt4 = ring_buffer_with_capacity<int, 4, false, 1024 * 1024>;
    using RingBufferDouble4 = ring_buffer_with_capacity<double, 4, false, 1024 * 1024>;
    using RingBufferComplex4 = ring_buffer_with_capacity<ComplexType, 4, false, 1024 * 1024>;
    using RingBufferTracked4 = ring_buffer_with_capacity<TrackedType, 4, false, 1024 * 1024>;

    // Medium capacity (non-power of 2)
    using RingBufferInt15 = ring_buffer_with_capacity<int, 15, false, 1024 * 1024>;
    using RingBufferDouble15 = ring_buffer_with_capacity<double, 15, false, 1024 * 1024>;
    using RingBufferComplex15 = ring_buffer_with_capacity<ComplexType, 15, false, 1024 * 1024>;
    using RingBufferTracked15 = ring_buffer_with_capacity<TrackedType, 15, false, 1024 * 1024>;

    // Larger capacity (power of 2)
    using RingBufferInt128 = ring_buffer_with_capacity<int, 128, false, 1024 * 1024>;
    using RingBufferDouble128 = ring_buffer_with_capacity<double, 128, false, 1024 * 1024>;
    using RingBufferComplex128 = ring_buffer_with_capacity<ComplexType, 128, false, 1024 * 1024>;
    using RingBufferTracked128 = ring_buffer_with_capacity<TrackedType, 128, false, 1024 * 1024>;

    // Edge case: capacity 1
    using RingBufferInt1 = ring_buffer_with_capacity<int, 1, false, 1024 * 1024>;
    using RingBufferTracked1 = ring_buffer_with_capacity<TrackedType, 1, false, 1024 * 1024>;

    // Heap allocation tests
    using RingBufferIntHeap = ring_buffer_with_capacity<int, 1000, true, 1024 * 1024>;
    using RingBufferComplexHeap = ring_buffer_with_capacity<ComplexType, 1000, true, 1024 * 1024>;

    // Group all types together
    using RingBufferTypes = ::testing::Types<
        RingBufferInt4,
        RingBufferDouble4,
        RingBufferComplex4,
        RingBufferTracked4,
        RingBufferInt15,
        RingBufferDouble15,
        RingBufferComplex15,
        RingBufferTracked15,
        RingBufferInt128,
        RingBufferDouble128,
        RingBufferComplex128,
        RingBufferTracked128,
        RingBufferInt1,
        RingBufferTracked1,
        RingBufferIntHeap,
        RingBufferComplexHeap>;

    // Instantiate the test suite for all RingBuffer types
    TYPED_TEST_SUITE(RingBufferTest, RingBufferTypes);

    // Include the test suite
#define RING_BUFFER_TEST_SUITE_NAME RingBufferTest
#include "test_suites/ring_buffer_test_suite.hpp"
#undef RING_BUFFER_TEST_SUITE_NAME

    // Overwriting Behavior Tests - Only for ring buffers that overwrite on full

    TYPED_TEST(RingBufferTest, PushBackOverwritesOldest)
    {
        if (this->Capacity <= 1)
        {
            GTEST_SKIP() << "Test requires capacity > 1";
        }

        // Fill to capacity
        for (size_t i = 0; i < this->Capacity; ++i)
        {
            this->buffer->push_back(this->make_value(static_cast<int>(i)));
        }

        // Overwrite - should remove oldest (0)
        this->buffer->push_back(this->make_value(100));

        EXPECT_TRUE(this->buffer->full());
        EXPECT_EQ(this->buffer->size(), this->Capacity);
        EXPECT_TRUE(this->verify_value(this->buffer->front(), 1)); // 0 was overwritten
        EXPECT_TRUE(this->verify_value(this->buffer->back(), 100));
    }

    TYPED_TEST(RingBufferTest, PushFrontOverwritesNewest)
    {
        if (this->Capacity <= 1)
        {
            GTEST_SKIP() << "Test requires capacity > 1";
        }

        // Fill to capacity with push_back
        for (size_t i = 0; i < this->Capacity; ++i)
        {
            this->buffer->push_back(this->make_value(static_cast<int>(i)));
        }

        // Push front should overwrite newest (back)
        this->buffer->push_front(this->make_value(100));

        EXPECT_TRUE(this->buffer->full());
        EXPECT_EQ(this->buffer->size(), this->Capacity);
        EXPECT_TRUE(this->verify_value(this->buffer->front(), 100));
        EXPECT_TRUE(this->verify_value(this->buffer->back(), static_cast<int>(this->Capacity - 2)));
    }

    TYPED_TEST(RingBufferTest, ManyOverwrites)
    {
        const int iterations = static_cast<int>(this->Capacity) * 2;

        for (int i = 0; i < iterations; ++i)
        {
            this->buffer->push_back(this->make_value(i));
        }

        EXPECT_TRUE(this->buffer->full());
        EXPECT_EQ(this->buffer->size(), this->Capacity);

        // Should contain last Capacity elements
        int start = iterations - static_cast<int>(this->Capacity);
        for (size_t i = 0; i < this->Capacity; ++i)
        {
            EXPECT_TRUE(this->verify_value((*this->buffer)[i], start + static_cast<int>(i)));
        }
    }

    TYPED_TEST(RingBufferTest, OverwritePreservesFifoOrder)
    {
        if (this->Capacity <= 1)
        {
            GTEST_SKIP() << "Test requires capacity > 1";
        }

        // Fill to capacity
        for (size_t i = 0; i < this->Capacity; ++i)
        {
            this->buffer->push_back(this->make_value(static_cast<int>(i)));
        }

        // Overwrite several times
        for (int i = 0; i < 3; ++i)
        {
            this->buffer->push_back(this->make_value(100 + i));
        }

        // Should still have FIFO ordering
        EXPECT_EQ(this->buffer->size(), this->Capacity);

        // First element should be (3) since 0-2 were overwritten
        EXPECT_TRUE(this->verify_value(this->buffer->front(), 3));

        // Last element should be 102
        EXPECT_TRUE(this->verify_value(this->buffer->back(), 102));

        // Verify all elements are in order
        int expected_start = 3;
        for (size_t i = 0; i < this->Capacity; ++i)
        {
            int expected_val = (i < this->Capacity - 3)
                                   ? (expected_start + static_cast<int>(i))
                                   : (100 + static_cast<int>(i) - (this->Capacity - 3));
            EXPECT_TRUE(this->verify_value((*this->buffer)[i], expected_val));
        }
    }

    TYPED_TEST(RingBufferTest, CapacityOneOverwrite)
    {
        if (this->Capacity == 1)
        {
            this->buffer->push_back(this->make_value(1));
            EXPECT_TRUE(this->buffer->full());
            EXPECT_EQ(this->buffer->size(), 1);

            // Overwrite
            this->buffer->push_back(this->make_value(2));
            EXPECT_TRUE(this->buffer->full());
            EXPECT_TRUE(this->verify_value(this->buffer->front(), 2));

            // Overwrite again
            this->buffer->push_front(this->make_value(3));
            EXPECT_TRUE(this->buffer->full());
            EXPECT_TRUE(this->verify_value(this->buffer->front(), 3));
        }
    }

    TYPED_TEST(RingBufferTest, NoLeaksOnOverwrite)
    {
        if constexpr (std::is_same_v<typename TestFixture::ValueType, TrackedType>)
        {
            reset_tracking();

            // Fill to capacity
            for (size_t i = 0; i < this->Capacity; ++i)
            {
                this->buffer->push_back(this->make_value(static_cast<int>(i)));
            }

            EXPECT_EQ(constructions.load(), static_cast<int>(this->Capacity));
            EXPECT_EQ(destructions.load(), 0);

            // Overwrite
            this->buffer->push_back(this->make_value(100));

            EXPECT_EQ(constructions.load(), static_cast<int>(this->Capacity) + 1);
            EXPECT_EQ(destructions.load(), 1); // One overwritten

            this->buffer->clear();
            EXPECT_FALSE(has_leaks());
        }
    }

    TYPED_TEST(RingBufferTest, MixedOverwritesWithPushFrontAndBack)
    {
        // Fill to capacity
        for (size_t i = 0; i < this->Capacity; ++i)
        {
            this->buffer->push_back(this->make_value(static_cast<int>(i)));
        }

        // Alternate overwrites from front and back
        this->buffer->push_back(this->make_value(100));  // Overwrites 0
        this->buffer->push_front(this->make_value(200)); // Overwrites Capacity-1
        this->buffer->push_back(this->make_value(101));  // Overwrites 1
        this->buffer->push_front(this->make_value(201)); // Overwrites Capacity-2

        EXPECT_TRUE(this->buffer->full());
        EXPECT_EQ(this->buffer->size(), this->Capacity);
        EXPECT_TRUE(this->verify_value(this->buffer->front(), 201));
    }

    TYPED_TEST(RingBufferTest, OverwriteCycleStressTest)
    {
        // Stress test: Fill, overwrite, pop, repeat
        for (int cycle = 0; cycle < 5; ++cycle)
        {
            // Fill to capacity
            for (size_t i = 0; i < this->Capacity; ++i)
            {
                this->buffer->push_back(this->make_value(static_cast<int>(cycle * 1000 + i)));
            }

            EXPECT_TRUE(this->buffer->full());

            // Overwrite half
            for (size_t i = 0; i < this->Capacity / 2; ++i)
            {
                this->buffer->push_back(this->make_value(static_cast<int>(cycle * 1000 + 100 + i)));
            }

            EXPECT_TRUE(this->buffer->full());

            // Pop half
            for (size_t i = 0; i < this->Capacity / 2; ++i)
            {
                this->buffer->pop_front();
            }
        }

        // Should still be in valid state
        EXPECT_LE(this->buffer->size(), this->Capacity);
        EXPECT_GT(this->buffer->size(), 0u);
    }

} // namespace mcds::tests
