#ifndef RING_BUFFER_TEST_SUITE_NAME
#error "Must define RING_BUFFER_TEST_SUITE_NAME before including this file"
#endif

// Initial State Tests
TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, InitiallyEmpty)
{
    EXPECT_TRUE(this->buffer->empty());
    EXPECT_FALSE(this->buffer->full());
    EXPECT_EQ(this->buffer->size(), 0);
    EXPECT_EQ(this->buffer->capacity(), this->Capacity);
}

// Push Back Tests
TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, PushBackSingleElement)
{
    this->buffer->push_back(this->make_value(42));

    EXPECT_FALSE(this->buffer->empty());
    EXPECT_EQ(this->buffer->size(), 1);
    EXPECT_TRUE(this->verify_value(this->buffer->front(), 42));
    EXPECT_TRUE(this->verify_value(this->buffer->back(), 42));
}

TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, PushBackMultipleElements)
{
    for (int i = 0; i < 5 && i < static_cast<int>(this->Capacity); ++i)
    {
        this->buffer->push_back(this->make_value(i));
    }

    int expected_size = std::min(5, static_cast<int>(this->Capacity));
    EXPECT_EQ(this->buffer->size(), expected_size);
    EXPECT_TRUE(this->verify_value(this->buffer->front(), 0));
    EXPECT_TRUE(this->verify_value(this->buffer->back(), expected_size - 1));
}

TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, PushBackToCapacity)
{
    for (size_t i = 0; i < this->Capacity; ++i)
    {
        this->buffer->push_back(this->make_value(static_cast<int>(i)));
    }

    EXPECT_TRUE(this->buffer->full());
    EXPECT_EQ(this->buffer->size(), this->Capacity);
    EXPECT_TRUE(this->verify_value(this->buffer->front(), 0));
    EXPECT_TRUE(this->verify_value(this->buffer->back(), static_cast<int>(this->Capacity - 1)));
}

// Push Front Tests
TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, PushFrontSingleElement)
{
    this->buffer->push_front(this->make_value(42));

    EXPECT_FALSE(this->buffer->empty());
    EXPECT_EQ(this->buffer->size(), 1);
    EXPECT_TRUE(this->verify_value(this->buffer->front(), 42));
    EXPECT_TRUE(this->verify_value(this->buffer->back(), 42));
}

TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, PushFrontMultipleElements)
{
    for (int i = 0; i < 5 && i < static_cast<int>(this->Capacity); ++i)
    {
        this->buffer->push_front(this->make_value(i));
    }

    int expected_size = std::min(5, static_cast<int>(this->Capacity));
    EXPECT_EQ(this->buffer->size(), expected_size);
    EXPECT_TRUE(this->verify_value(this->buffer->front(), expected_size - 1));
    EXPECT_TRUE(this->verify_value(this->buffer->back(), 0));
}

// Mixed Push Operations
TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, MixedPushFrontAndBack)
{
    if (this->Capacity <= 1)
    {
        GTEST_SKIP() << "Test requires capacity > 1";
    }

    this->buffer->push_back(this->make_value(1));
    this->buffer->push_back(this->make_value(2));
    this->buffer->push_front(this->make_value(0));
    this->buffer->push_back(this->make_value(3));

    EXPECT_EQ(this->buffer->size(), 4);
    EXPECT_TRUE(this->verify_value(this->buffer->front(), 0));
    EXPECT_TRUE(this->verify_value(this->buffer->back(), 3));
    EXPECT_TRUE(this->verify_value((*this->buffer)[1], 1));
    EXPECT_TRUE(this->verify_value((*this->buffer)[2], 2));
}

// Pop Front Tests
TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, PopFrontSingleElement)
{
    this->buffer->push_back(this->make_value(42));
    this->buffer->pop_front();

    EXPECT_TRUE(this->buffer->empty());
    EXPECT_EQ(this->buffer->size(), 0);
}

TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, PopFrontMultipleElements)
{
    if (this->Capacity <= 1)
    {
        GTEST_SKIP() << "Test requires capacity > 1";
    }

    for (int i = 0; i < 4; ++i)
    {
        this->buffer->push_back(this->make_value(i));
    }

    this->buffer->pop_front();
    this->buffer->pop_front();

    EXPECT_EQ(this->buffer->size(), 2);
    EXPECT_TRUE(this->verify_value(this->buffer->front(), 2));
    EXPECT_TRUE(this->verify_value(this->buffer->back(), 3));
}

TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, PopFrontOnEmptyIsNoOp)
{
    this->buffer->pop_front();
    EXPECT_TRUE(this->buffer->empty());
    EXPECT_EQ(this->buffer->size(), 0);
}

// Pop Back Tests
TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, PopBackSingleElement)
{
    this->buffer->push_back(this->make_value(42));
    this->buffer->pop_back();

    EXPECT_TRUE(this->buffer->empty());
    EXPECT_EQ(this->buffer->size(), 0);
}

TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, PopBackMultipleElements)
{
    if (this->Capacity <= 1)
    {
        GTEST_SKIP() << "Test requires capacity > 1";
    }

    for (int i = 0; i < 4; ++i)
    {
        this->buffer->push_back(this->make_value(i));
    }

    this->buffer->pop_back();
    this->buffer->pop_back();

    EXPECT_EQ(this->buffer->size(), 2);
    EXPECT_TRUE(this->verify_value(this->buffer->front(), 0));
    EXPECT_TRUE(this->verify_value(this->buffer->back(), 1));
}

TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, PopBackOnEmptyIsNoOp)
{
    this->buffer->pop_back();
    EXPECT_TRUE(this->buffer->empty());
    EXPECT_EQ(this->buffer->size(), 0);
}

// Mixed Pop Operations
TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, MixedPopFrontAndBack)
{
    if (this->Capacity <= 1)
    {
        GTEST_SKIP() << "Test requires capacity > 1";
    }

    for (int i = 0; i < 4; ++i)
    {
        this->buffer->push_back(this->make_value(i));
    }

    this->buffer->pop_front(); // Remove 0
    this->buffer->pop_back();  // Remove 4

    EXPECT_EQ(this->buffer->size(), 2);
    EXPECT_TRUE(this->verify_value(this->buffer->front(), 1));
    EXPECT_TRUE(this->verify_value(this->buffer->back(), 2));
}

// Random Access Tests
TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, RandomAccessIndexing)
{
    for (int i = 0; i < 5 && i < static_cast<int>(this->Capacity); ++i)
    {
        this->buffer->push_back(this->make_value(i * 10));
    }

    int size = std::min(5, static_cast<int>(this->Capacity));
    for (int i = 0; i < size; ++i)
    {
        EXPECT_TRUE(this->verify_value((*this->buffer)[i], i * 10));
    }
}

TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, RandomAccessAfterWrap)
{
    if (this->Capacity <= 1)
    {
        GTEST_SKIP() << "Test requires capacity > 1";
    }

    // Fill buffer
    for (size_t i = 0; i < this->Capacity; ++i)
    {
        this->buffer->push_back(this->make_value(static_cast<int>(i)));
    }

    // Pop some from front
    for (int i = 0; i < 3 && i < static_cast<int>(this->Capacity); ++i)
    {
        this->buffer->pop_front();
    }

    // Push more to wrap
    for (int i = 0; i < 3 && i < static_cast<int>(this->Capacity); ++i)
    {
        this->buffer->push_back(this->make_value(100 + i));
    }

    // Verify indexing works correctly
    EXPECT_TRUE(this->verify_value((*this->buffer)[0], 3)); // First element now
    EXPECT_TRUE(this->verify_value(this->buffer->front(), 3));
}

// Clear Tests
TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, ClearEmptiesBuffer)
{
    for (int i = 0; i < 5 && i < static_cast<int>(this->Capacity); ++i)
    {
        this->buffer->push_back(this->make_value(i));
    }

    this->buffer->clear();

    EXPECT_TRUE(this->buffer->empty());
    EXPECT_EQ(this->buffer->size(), 0);
    EXPECT_FALSE(this->buffer->full());
}

TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, ClearOnEmptyIsNoOp)
{
    this->buffer->clear();
    EXPECT_TRUE(this->buffer->empty());
}

TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, ReusableAfterClear)
{
    for (int i = 0; i < 5 && i < static_cast<int>(this->Capacity); ++i)
    {
        this->buffer->push_back(this->make_value(i));
    }

    this->buffer->clear();

    this->buffer->push_back(this->make_value(42));
    EXPECT_EQ(this->buffer->size(), 1);
    EXPECT_TRUE(this->verify_value(this->buffer->front(), 42));
}

// FIFO Usage Pattern
TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, FifoOrdering)
{
    constexpr int count = 10;
    for (int i = 0; i < count && i < static_cast<int>(this->Capacity); ++i)
    {
        this->buffer->push_back(this->make_value(i));
    }

    int iterations = std::min(count, static_cast<int>(this->Capacity));
    for (int i = 0; i < iterations; ++i)
    {
        EXPECT_TRUE(this->verify_value(this->buffer->front(), i));
        this->buffer->pop_front();
    }

    EXPECT_TRUE(this->buffer->empty());
}

// LIFO Usage Pattern (Stack)
TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, LifoOrdering)
{
    constexpr int count = 10;
    for (int i = 0; i < count && i < static_cast<int>(this->Capacity); ++i)
    {
        this->buffer->push_back(this->make_value(i));
    }

    int iterations = std::min(count, static_cast<int>(this->Capacity));
    for (int i = iterations - 1; i >= 0; --i)
    {
        EXPECT_TRUE(this->verify_value(this->buffer->back(), i));
        this->buffer->pop_back();
    }

    EXPECT_TRUE(this->buffer->empty());
}

// Emplace Tests
TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, EmplaceBackInPlace)
{
    if constexpr (std::is_same_v<typename TestFixture::ValueType, ComplexType>)
    {
        this->buffer->emplace_back(1, 10, "test");

        EXPECT_EQ(this->buffer->size(), 1);
        EXPECT_EQ(this->buffer->front().primary_id, 1);
        EXPECT_EQ(this->buffer->front().secondary_id, 10);
        EXPECT_EQ(this->buffer->front().label, "test");
    }
}

TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, EmplaceFrontInPlace)
{
    if constexpr (std::is_same_v<typename TestFixture::ValueType, ComplexType>)
    {
        this->buffer->emplace_front(1, 10, "test");

        EXPECT_EQ(this->buffer->size(), 1);
        EXPECT_EQ(this->buffer->front().primary_id, 1);
        EXPECT_EQ(this->buffer->front().secondary_id, 10);
        EXPECT_EQ(this->buffer->front().label, "test");
    }
}

// Edge Cases
TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, CapacityOneBuffer)
{
    if (this->Capacity == 1)
    {
        this->buffer->push_back(this->make_value(1));
        EXPECT_TRUE(this->buffer->full());
        EXPECT_EQ(this->buffer->size(), 1);
    }
}

TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, AlternatingPushPop)
{
    for (int i = 0; i < 20; ++i)
    {
        if (!this->buffer->full())
        {
            this->buffer->push_back(this->make_value(i));
        }
        if (i % 2 == 1 && !this->buffer->empty())
        {
            this->buffer->pop_front();
        }
    }

    EXPECT_LE(this->buffer->size(), this->Capacity);
}

// Resource Management Tests (TrackedType only)
TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, NoLeaksOnPushPop)
{
    if constexpr (std::is_same_v<typename TestFixture::ValueType, TrackedType>)
    {
        reset_tracking();

        for (int i = 0; i < 5 && i < static_cast<int>(this->Capacity); ++i)
        {
            this->buffer->push_back(this->make_value(i));
        }

        int pushed = std::min(5, static_cast<int>(this->Capacity));
        EXPECT_EQ(constructions.load(), pushed);

        this->buffer->pop_front();
        EXPECT_EQ(destructions.load(), 1);

        this->buffer->clear();
        EXPECT_EQ(destructions.load(), pushed);
        EXPECT_FALSE(has_leaks());
    }
}

TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, DestructorCleansUpAll)
{
    if constexpr (std::is_same_v<typename TestFixture::ValueType, TrackedType>)
    {
        reset_tracking();

        {
            auto temp_buffer = std::make_unique<typename TestFixture::RingBufferType>();
            for (int i = 0; i < 5 && i < static_cast<int>(this->Capacity); ++i)
            {
                temp_buffer->push_back(this->make_value(i));
            }

            int pushed = std::min(5, static_cast<int>(this->Capacity));
            EXPECT_EQ(constructions.load(), pushed);
        } // Buffer destroyed here

        EXPECT_FALSE(has_leaks());
    }
}

// Wrap-Around Tests
TYPED_TEST(RING_BUFFER_TEST_SUITE_NAME, WrapAroundCorrectness)
{
    // Fill and empty multiple times
    for (int cycle = 0; cycle < 3; ++cycle)
    {
        // Fill partially
        size_t to_push = std::min(this->Capacity, static_cast<size_t>(10));
        for (size_t i = 0; i < to_push; ++i)
        {
            if (!this->buffer->full())
            {
                this->buffer->push_back(this->make_value(static_cast<int>(cycle * 100 + i)));
            }
        }

        // Pop half
        for (size_t i = 0; i < to_push / 2 && !this->buffer->empty(); ++i)
        {
            this->buffer->pop_front();
        }
    }

    // Verify buffer is still in valid state
    EXPECT_LE(this->buffer->size(), this->Capacity);
}