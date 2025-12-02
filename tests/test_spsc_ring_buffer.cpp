#include <gtest/gtest.h>

#include "mcds/spsc_ring_buffer.hpp"
#include "test_types/custom_types.hpp"
#include "test_suites/ring_buffer_typed_tests.hpp"

namespace mcds::tests
{
    template <typename T, size_t Cap, bool ForceHeap = false, size_t StackBudget = 1024 * 1024>
    struct spsc_ring_buffer_with_capacity : public spsc_ring_buffer<T, Cap, ForceHeap, StackBudget>
    {
        using value_type = T;
        static constexpr size_t capacity_value = Cap; // Full capacity usable
    };

    template <typename RingBuffer>
    class SPSCRingBufferTest : public RingBufferTypedTest<RingBuffer>
    {
    protected:
        using RingBufferType = RingBuffer;
    };

    // SPSC configurations
    using SPSCInt4 = spsc_ring_buffer_with_capacity<int, 4>;
    using SPSCDouble4 = spsc_ring_buffer_with_capacity<double, 4>;
    using SPSCComplex4 = spsc_ring_buffer_with_capacity<ComplexType, 4>;
    using SPSCTracked4 = spsc_ring_buffer_with_capacity<TrackedType, 4>;

    using SPSCInt16 = spsc_ring_buffer_with_capacity<int, 16>;
    using SPSCDouble16 = spsc_ring_buffer_with_capacity<double, 16>;
    using SPSCComplex16 = spsc_ring_buffer_with_capacity<ComplexType, 16>;
    using SPSCTracked16 = spsc_ring_buffer_with_capacity<TrackedType, 16>;

    using SPSCInt128 = spsc_ring_buffer_with_capacity<int, 128>;
    using SPSCDouble128 = spsc_ring_buffer_with_capacity<double, 128>;
    using SPSCComplex128 = spsc_ring_buffer_with_capacity<ComplexType, 128>;
    using SPSCTracked128 = spsc_ring_buffer_with_capacity<TrackedType, 128>;

    // Heap allocation tests
    using SPSCIntHeap = spsc_ring_buffer_with_capacity<int, 1024, true>;
    using SPSCComplexHeap = spsc_ring_buffer_with_capacity<ComplexType, 1024, true>;

    using SPSCRingBufferTypes = ::testing::Types<
        SPSCInt4,
        SPSCDouble4,
        SPSCComplex4,
        SPSCTracked4,
        SPSCInt16,
        SPSCDouble16,
        SPSCComplex16,
        SPSCTracked16,
        SPSCInt128,
        SPSCDouble128,
        SPSCComplex128,
        SPSCTracked128,
        SPSCIntHeap,
        SPSCComplexHeap>;

    TYPED_TEST_SUITE(SPSCRingBufferTest, SPSCRingBufferTypes);

    // Use generic ring buffer test suite
#define RING_BUFFER_TEST_SUITE_NAME SPSCRingBufferTest
#include "test_suites/ring_buffer_test_suite.hpp"
#undef RING_BUFFER_TEST_SUITE_NAME

    // SPSC-specific tests: returns false/nullopt instead of overwriting

    TYPED_TEST(SPSCRingBufferTest, EmplaceBackReturnsFalseWhenFull)
    {
        for (size_t i = 0; i < this->Capacity; ++i)
        {
            EXPECT_TRUE(this->buffer->emplace_back(this->make_value(static_cast<int>(i))));
        }

        EXPECT_TRUE(this->buffer->full());
        EXPECT_FALSE(this->buffer->emplace_back(this->make_value(999)));
        EXPECT_EQ(this->buffer->size(), this->Capacity);
    }

    TYPED_TEST(SPSCRingBufferTest, EmplaceFrontReturnsFalseWhenFull)
    {
        for (size_t i = 0; i < this->Capacity; ++i)
        {
            EXPECT_TRUE(this->buffer->emplace_front(this->make_value(static_cast<int>(i))));
        }

        EXPECT_TRUE(this->buffer->full());
        EXPECT_FALSE(this->buffer->emplace_front(this->make_value(999)));
        EXPECT_EQ(this->buffer->size(), this->Capacity);
    }

    TYPED_TEST(SPSCRingBufferTest, PushBackReturnsFalseWhenFull)
    {
        for (size_t i = 0; i < this->Capacity; ++i)
        {
            EXPECT_TRUE(this->buffer->push_back(this->make_value(static_cast<int>(i))));
        }

        EXPECT_TRUE(this->buffer->full());
        EXPECT_FALSE(this->buffer->push_back(this->make_value(999)));
    }

    TYPED_TEST(SPSCRingBufferTest, PushFrontReturnsFalseWhenFull)
    {
        for (size_t i = 0; i < this->Capacity; ++i)
        {
            EXPECT_TRUE(this->buffer->push_front(this->make_value(static_cast<int>(i))));
        }

        EXPECT_TRUE(this->buffer->full());
        EXPECT_FALSE(this->buffer->push_front(this->make_value(999)));
    }

    TYPED_TEST(SPSCRingBufferTest, PopFrontReturnsNulloptWhenEmpty)
    {
        EXPECT_TRUE(this->buffer->empty());
        auto result = this->buffer->pop_front();
        EXPECT_FALSE(result.has_value());
    }

    TYPED_TEST(SPSCRingBufferTest, PopBackReturnsNulloptWhenEmpty)
    {
        EXPECT_TRUE(this->buffer->empty());
        auto result = this->buffer->pop_back();
        EXPECT_FALSE(result.has_value());
    }

    TYPED_TEST(SPSCRingBufferTest, PopFrontReturnsValueWhenAvailable)
    {
        this->buffer->emplace_back(this->make_value(42));
        auto result = this->buffer->pop_front();

        ASSERT_TRUE(result.has_value());
        EXPECT_TRUE(this->verify_value(*result, 42));
    }

    TYPED_TEST(SPSCRingBufferTest, PopBackReturnsValueWhenAvailable)
    {
        this->buffer->emplace_back(this->make_value(42));
        auto result = this->buffer->pop_back();

        ASSERT_TRUE(result.has_value());
        EXPECT_TRUE(this->verify_value(*result, 42));
    }

    TYPED_TEST(SPSCRingBufferTest, MoveConstructor)
    {
        for (int i = 0; i < 5 && i < static_cast<int>(this->Capacity); ++i)
        {
            this->buffer->emplace_back(this->make_value(i));
        }

        size_t original_size = this->buffer->size();
        auto moved_buffer = std::move(*this->buffer);

        EXPECT_EQ(moved_buffer.size(), original_size);
        EXPECT_EQ(this->buffer->size(), 0);
        EXPECT_TRUE(this->buffer->empty());
    }

    TYPED_TEST(SPSCRingBufferTest, MoveAssignment)
    {
        for (int i = 0; i < 5 && i < static_cast<int>(this->Capacity); ++i)
        {
            this->buffer->emplace_back(this->make_value(i));
        }

        size_t original_size = this->buffer->size();

        using BufferType = typename TestFixture::RingBufferType;
        BufferType dest_buffer;
        dest_buffer.emplace_back(this->make_value(99));

        dest_buffer = std::move(*this->buffer);

        EXPECT_EQ(dest_buffer.size(), original_size);
        EXPECT_TRUE(this->buffer->empty());
    }

} // namespace mcds::tests
