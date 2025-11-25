#include <gtest/gtest.h>

#include "mcds/ring_buffer.hpp"
#include "test_types/custom_types.hpp"

namespace mcds::tests
{
    // Basic trivial type behaviour
    TEST(RingBufferInt, InitiallyEmpty)
    {
        ring_buffer<int, 4> buf;

        EXPECT_TRUE(buf.empty());
        EXPECT_FALSE(buf.full());
        EXPECT_EQ(buf.size(), 0u);
        EXPECT_EQ(buf.capacity(), 4u);
    }

    TEST(RingBufferInt, PushAndPopWithoutWrap)
    {
        ring_buffer<int, 4> buf;

        buf.push(10);
        buf.push(20);

        EXPECT_FALSE(buf.empty());
        EXPECT_EQ(buf.size(), 2u);
        EXPECT_EQ(buf.front(), 10);
        EXPECT_EQ(buf.back(), 20);

        buf.pop_front();
        EXPECT_EQ(buf.size(), 1u);
        EXPECT_EQ(buf.front(), 20);
        EXPECT_EQ(buf.back(), 20);

        buf.pop_front();
        EXPECT_TRUE(buf.empty());
        EXPECT_EQ(buf.size(), 0u);

        // Popping empty is a no op
        buf.pop_front();
        EXPECT_TRUE(buf.empty());
        EXPECT_EQ(buf.size(), 0u);
    }

    TEST(RingBufferInt, WrapAroundPowerOfTwoCapacity)
    {
        ring_buffer<int, 4> buf;

        // Fill fully
        buf.push(1);
        buf.push(2);
        buf.push(3);
        buf.push(4);

        ASSERT_TRUE(buf.full());
        EXPECT_EQ(buf.size(), 4u);
        EXPECT_EQ(buf.front(), 1);
        EXPECT_EQ(buf.back(), 4);

        // Move head forward
        buf.pop_front(); // drop 1
        buf.pop_front(); // drop 2

        EXPECT_EQ(buf.size(), 2u);
        EXPECT_EQ(buf.front(), 3);
        EXPECT_EQ(buf.back(), 4);

        // Force tail wrap
        buf.push(5);
        buf.push(6);

        ASSERT_TRUE(buf.full());
        EXPECT_EQ(buf.size(), 4u);

        // Logical ordering must be FIFO
        EXPECT_EQ(buf.front(), 3);
        EXPECT_EQ(buf[0], 3);
        EXPECT_EQ(buf[1], 4);
        EXPECT_EQ(buf[2], 5);
        EXPECT_EQ(buf[3], 6);
        EXPECT_EQ(buf.back(), 6);
    }

    TEST(RingBufferInt, WrapAroundNonPowerOfTwoCapacity)
    {
        ring_buffer<int, 5> buf;

        buf.push(10);
        buf.push(20);
        buf.push(30);
        buf.push(40);
        buf.push(50);

        ASSERT_TRUE(buf.full());
        EXPECT_EQ(buf.front(), 10);
        EXPECT_EQ(buf.back(), 50);

        buf.pop_front(); // drop 10
        buf.pop_front(); // drop 20

        EXPECT_EQ(buf.size(), 3u);
        EXPECT_EQ(buf.front(), 30);

        // Force modulo-based wrap
        buf.push(60);
        buf.push(70);

        ASSERT_TRUE(buf.full());
        EXPECT_EQ(buf.size(), 5u);

        // Logical contents
        EXPECT_EQ(buf.front(), 30);
        EXPECT_EQ(buf[0], 30);
        EXPECT_EQ(buf[1], 40);
        EXPECT_EQ(buf[2], 50);
        EXPECT_EQ(buf[3], 60);
        EXPECT_EQ(buf[4], 70);
        EXPECT_EQ(buf.back(), 70);
    }

    TEST(RingBufferInt, OverwriteOnFullDropsOldest)
    {
        ring_buffer<int, 3> buf;

        buf.push(1);
        buf.push(2);
        buf.push(3);

        ASSERT_TRUE(buf.full());
        EXPECT_EQ(buf.size(), 3u);
        EXPECT_EQ(buf.front(), 1);
        EXPECT_EQ(buf.back(), 3);

        // Overwrite 1
        buf.push(4);
        ASSERT_TRUE(buf.full());
        EXPECT_EQ(buf.size(), 3u);

        EXPECT_EQ(buf.front(), 2);
        EXPECT_EQ(buf[0], 2);
        EXPECT_EQ(buf[1], 3);
        EXPECT_EQ(buf[2], 4);
        EXPECT_EQ(buf.back(), 4);

        // Overwrite 2
        buf.push(5);
        EXPECT_EQ(buf.front(), 3);
        EXPECT_EQ(buf[0], 3);
        EXPECT_EQ(buf[1], 4);
        EXPECT_EQ(buf[2], 5);
        EXPECT_EQ(buf.back(), 5);
    }

    TEST(RingBufferInt, ClearResetsStateAndIsReUsable)
    {
        ring_buffer<int, 4> buf;

        buf.push(1);
        buf.push(2);
        buf.push(3);

        EXPECT_FALSE(buf.empty());
        EXPECT_EQ(buf.size(), 3u);

        buf.clear();
        EXPECT_TRUE(buf.empty());
        EXPECT_EQ(buf.size(), 0u);
        EXPECT_FALSE(buf.full());

        // Reuse after clear
        buf.push(42);
        EXPECT_FALSE(buf.empty());
        EXPECT_EQ(buf.size(), 1u);
        EXPECT_EQ(buf.front(), 42);
        EXPECT_EQ(buf.back(), 42);
    }

    TEST(RingBufferInt, IndexingAfterHeadMovementAndWrap)
    {
        ring_buffer<int, 4> buf;

        buf.push(1);
        buf.push(2);
        buf.push(3);
        buf.push(4);

        buf.pop_front(); // drop 1
        buf.pop_front(); // drop 2

        // Now size=2
        EXPECT_EQ(buf.front(), 3);
        EXPECT_EQ(buf[0], 3);
        EXPECT_EQ(buf[1], 4);

        buf.push(5);
        buf.push(6); // wrap

        ASSERT_TRUE(buf.full());
        // logical [3,4,5,6]
        EXPECT_EQ(buf[0], 3);
        EXPECT_EQ(buf[1], 4);
        EXPECT_EQ(buf[2], 5);
        EXPECT_EQ(buf[3], 6);
    }

    TEST(RingBufferInt, CapacityOneBehaviour)
    {
        ring_buffer<int, 1> buf;

        EXPECT_TRUE(buf.empty());
        EXPECT_EQ(buf.capacity(), 1u);

        buf.push(10);
        EXPECT_TRUE(buf.full());
        EXPECT_EQ(buf.size(), 1u);
        EXPECT_EQ(buf.front(), 10);
        EXPECT_EQ(buf.back(), 10);

        // Every push overwrites the single slot
        buf.push(20);
        EXPECT_TRUE(buf.full());
        EXPECT_EQ(buf.size(), 1u);
        EXPECT_EQ(buf.front(), 20);
        EXPECT_EQ(buf.back(), 20);

        buf.pop_front();
        EXPECT_TRUE(buf.empty());
    }

    // ComplexType tests
    TEST(RingBufferComplexType, PreservesOrderingAndContent)
    {
        ring_buffer<ComplexType, 4> buf;

        buf.emplace(1, 10, "one");
        buf.emplace(2, 20, "two");
        buf.emplace(3, 30, "three");

        EXPECT_EQ(buf.size(), 3u);
        EXPECT_EQ(buf.front().primary_id, 1);
        EXPECT_EQ(buf.front().secondary_id, 10);
        EXPECT_EQ(buf.front().label, "one");
        EXPECT_EQ(buf.back().primary_id, 3);
        EXPECT_EQ(buf.back().label, "three");

        // Overwrite case
        buf.emplace(4, 40, "four");
        buf.emplace(5, 50, "five"); // overwrites 1

        EXPECT_EQ(buf.size(), 4u);
        EXPECT_EQ(buf.front().primary_id, 2);
        EXPECT_EQ(buf[0].label, "two");
        EXPECT_EQ(buf[1].label, "three");
        EXPECT_EQ(buf[2].label, "four");
        EXPECT_EQ(buf[3].label, "five");
        EXPECT_EQ(buf.back().primary_id, 5);
    }

    // TrackedType + leak detection
    TEST(RingBufferTrackedType, NoLeaksOnPushPopAndClear)
    {
        reset_tracking();

        {
            ring_buffer<TrackedType, 4> buf;
            EXPECT_EQ(constructions.load(), 0);
            EXPECT_EQ(destructions.load(), 0);

            buf.emplace(1); // alloc resource
            buf.emplace(2);
            buf.emplace(3);

            EXPECT_EQ(buf.size(), 3u);
            EXPECT_EQ(constructions.load(), 3);
            EXPECT_EQ(destructions.load(), 0);

            // Pop one -> one resource should be destroyed
            buf.pop_front();
            EXPECT_EQ(buf.size(), 2u);
            EXPECT_EQ(destructions.load(), 1);

            // Clear should destroy the remaining two
            buf.clear();
            EXPECT_TRUE(buf.empty());
            EXPECT_EQ(destructions.load(), 3);
            EXPECT_FALSE(has_leaks());
        }

        // After buffer destruction there should still be no leaks
        EXPECT_FALSE(has_leaks());
        EXPECT_EQ(leaked_objects(), 0);
    }

    TEST(RingBufferTrackedType, NoLeaksOnOverwriteWhenFull)
    {
        reset_tracking();

        {
            ring_buffer<TrackedType, 3> buf;

            buf.emplace(1);
            buf.emplace(2);
            buf.emplace(3);

            EXPECT_EQ(buf.size(), 3u);
            EXPECT_EQ(constructions.load(), 3);
            EXPECT_EQ(destructions.load(), 0);

            // Overwrite full buffer
            buf.emplace(4);
            EXPECT_EQ(buf.size(), 3u);
            EXPECT_EQ(destructions.load(), 1);

            buf.emplace(5);
            EXPECT_EQ(destructions.load(), 2);

            // Sanity check ordering
            EXPECT_EQ(buf.front().id, 3); // 1,2 dropped
            EXPECT_EQ(buf.back().id, 5);
        }

        // All TrackedResource objects should be cleaned up
        EXPECT_FALSE(has_leaks());
        EXPECT_EQ(leaked_objects(), 0);
    }

    TEST(RingBufferTrackedType, CopyAndMoveSemanticsWork)
    {
        reset_tracking();

        ring_buffer<TrackedType, 4> buf;

        TrackedType t1(1);
        EXPECT_EQ(constructions.load(), 1); // resource for t1

        // Push by const reference -> copy construct TrackedType + allocate new resource
        buf.push(t1);
        EXPECT_EQ(buf.size(), 1u);
        EXPECT_EQ(constructions.load(), 2);
        EXPECT_EQ(destructions.load(), 0);

        int original_resource_value = t1.get_resource_value();
        EXPECT_EQ(buf.front().get_resource_value(), original_resource_value);

        // Push by rvalue -> move construct TrackedType into buffer (resource moved)
        buf.push(TrackedType(2));
        // One resource for temp + one move into buffer
        EXPECT_EQ(constructions.load(), 3);
        EXPECT_EQ(buf.size(), 2u);

        // Ensure contents are as expected
        EXPECT_EQ(buf[0].id, 1);
        EXPECT_EQ(buf[1].id, 2);

        // Clear and check all resources are freed
        buf.clear();
        EXPECT_TRUE(buf.empty());
        EXPECT_EQ(destructions.load(), 2);
    }

    TEST(RingBufferTrackedType, FrontAndBackTrackFifoOrder)
    {
        reset_tracking();

        ring_buffer<TrackedType, 4> buf;

        buf.emplace(1);
        buf.emplace(2);
        buf.emplace(3);

        EXPECT_EQ(buf.front().id, 1);
        EXPECT_EQ(buf.back().id, 3);

        buf.pop_front(); // drop id 1
        EXPECT_EQ(buf.front().id, 2);
        EXPECT_EQ(buf.back().id, 3);

        buf.emplace(4);
        EXPECT_EQ(buf.front().id, 2);
        EXPECT_EQ(buf.back().id, 4);

        // Overwrite full buffer and check
        buf.emplace(5); // may overwrite depending on size
        EXPECT_EQ(buf.back().id, 5);

        // Just ensure we don't leak
        buf.clear();
        EXPECT_FALSE(has_leaks());
    }

    // Add these two tests to your existing ring_buffer test suite

    TEST(RingBufferInt, PopBackBasicBehaviour)
    {
        ring_buffer<int, 4> buf;

        buf.push(10);
        buf.push(20);
        buf.push(30);

        EXPECT_EQ(buf.size(), 3u);
        EXPECT_EQ(buf.front(), 10);
        EXPECT_EQ(buf.back(), 30);

        // Pop from back
        buf.pop_back();
        EXPECT_EQ(buf.size(), 2u);
        EXPECT_EQ(buf.front(), 10);
        EXPECT_EQ(buf.back(), 20);

        // Pop from back again
        buf.pop_back();
        EXPECT_EQ(buf.size(), 1u);
        EXPECT_EQ(buf.front(), 10);
        EXPECT_EQ(buf.back(), 10);

        // Pop last element
        buf.pop_back();
        EXPECT_TRUE(buf.empty());
        EXPECT_EQ(buf.size(), 0u);

        // Popping empty is a no-op
        buf.pop_back();
        EXPECT_TRUE(buf.empty());
        EXPECT_EQ(buf.size(), 0u);
    }

    TEST(RingBufferTrackedType, PopBackNoLeaks)
    {
        reset_tracking();

        {
            ring_buffer<TrackedType, 4> buf;

            buf.emplace(1);
            buf.emplace(2);
            buf.emplace(3);
            buf.emplace(4);

            EXPECT_EQ(buf.size(), 4u);
            EXPECT_EQ(constructions.load(), 4);
            EXPECT_EQ(destructions.load(), 0);

            // Pop from back should destroy element 4
            buf.pop_back();
            EXPECT_EQ(buf.size(), 3u);
            EXPECT_EQ(destructions.load(), 1);
            EXPECT_EQ(buf.back().id, 3);

            // Pop from back should destroy element 3
            buf.pop_back();
            EXPECT_EQ(buf.size(), 2u);
            EXPECT_EQ(destructions.load(), 2);
            EXPECT_EQ(buf.back().id, 2);

            // Pop from front should destroy element 1
            buf.pop_front();
            EXPECT_EQ(buf.size(), 1u);
            EXPECT_EQ(destructions.load(), 3);
            EXPECT_EQ(buf.front().id, 2);
            EXPECT_EQ(buf.back().id, 2);

            // Clear remaining element
            buf.clear();
            EXPECT_EQ(destructions.load(), 4);
        }

        EXPECT_FALSE(has_leaks());
        EXPECT_EQ(leaked_objects(), 0);
    }

} // namespace mcds::tests
