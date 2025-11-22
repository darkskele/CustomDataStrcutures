#include <gtest/gtest.h>
#include <cstdint>
#include <vector>
#include <algorithm>

#include "mcds/memory/slab.hpp"
#include "test_types/custom_types.hpp"

namespace mcds::tests
{

    // Basic Functionality Tests
    TEST(SlabTest, ConstructorInitializesFreelist)
    {
        memory::Slab<int, uint8_t> slab;

        EXPECT_EQ(slab.capacity(), 256);
        EXPECT_EQ(slab.size(), 0);
        EXPECT_TRUE(slab.has_free());
    }

    TEST(SlabTest, AllocateSingleObject)
    {
        memory::Slab<int, uint8_t> slab;

        auto idx = slab.allocate(42);

        EXPECT_EQ(slab.size(), 1);
        EXPECT_EQ(slab[idx], 42);
    }

    TEST(SlabTest, AllocateAndDeallocate)
    {
        memory::Slab<int, uint8_t> slab;

        auto idx = slab.allocate(100);
        EXPECT_EQ(slab.size(), 1);
        EXPECT_EQ(slab[idx], 100);

        slab.deallocate(idx);
        EXPECT_EQ(slab.size(), 0);
    }

    TEST(SlabTest, AllocateMultipleObjects)
    {
        memory::Slab<int, uint8_t> slab;
        std::vector<uint8_t> indices;

        for (int i = 0; i < 10; ++i)
        {
            auto idx = slab.allocate(i * 10);
            indices.push_back(idx);
        }

        EXPECT_EQ(slab.size(), 10);

        // Verify all values
        for (size_t i = 0; i < indices.size(); ++i)
        {
            EXPECT_EQ(slab[indices[i]], static_cast<int>(i * 10));
        }
    }

    TEST(SlabTest, ReusesDeallocatedSlots)
    {
        memory::Slab<int, uint8_t> slab;

        // Allocate and deallocate
        auto idx1 = slab.allocate(100);
        slab.deallocate(idx1);

        // Next allocation should reuse the slot
        auto idx2 = slab.allocate(200);
        EXPECT_EQ(idx1, idx2); // Should be same slot
        EXPECT_EQ(slab[idx2], 200);
    }

    // Capacity and Edge Cases
    TEST(SlabTest, FillEntireCapacity_uint8_t)
    {
        memory::Slab<int, uint8_t> slab;
        std::vector<uint8_t> indices;

        // Fill all 256 slots
        for (int i = 0; i < 256; ++i)
        {
            auto idx = slab.allocate(i);
            indices.push_back(idx);
        }

        EXPECT_EQ(slab.size(), 256);
        EXPECT_FALSE(slab.has_free());

        // Verify all values are correct
        for (size_t i = 0; i < indices.size(); ++i)
        {
            EXPECT_EQ(slab[indices[i]], static_cast<int>(i));
        }
    }

    TEST(SlabTest, DeallocateAllAndRefill)
    {
        memory::Slab<int, uint8_t> slab;
        std::vector<uint8_t> indices;

        // Fill completely
        for (int i = 0; i < 256; ++i)
        {
            indices.push_back(slab.allocate(i));
        }

        EXPECT_EQ(slab.size(), 256);

        // Deallocate all
        for (auto idx : indices)
        {
            slab.deallocate(idx);
        }

        EXPECT_EQ(slab.size(), 0);
        EXPECT_TRUE(slab.has_free());

        // Refill
        indices.clear();
        for (int i = 0; i < 256; ++i)
        {
            indices.push_back(slab.allocate(i * 2));
        }

        EXPECT_EQ(slab.size(), 256);

        // Verify new values
        for (size_t i = 0; i < indices.size(); ++i)
        {
            EXPECT_EQ(slab[indices[i]], static_cast<int>(i * 2));
        }
    }

    TEST(SlabTest, AlternatingAllocateAndDeallocate)
    {
        memory::Slab<int, uint8_t> slab;

        for (int i = 0; i < 100; ++i)
        {
            auto idx = slab.allocate(i);
            EXPECT_EQ(slab[idx], i);
            EXPECT_EQ(slab.size(), 1);

            slab.deallocate(idx);
            EXPECT_EQ(slab.size(), 0);
        }
    }

    TEST(SlabTest, SparseAllocationPattern)
    {
        memory::Slab<int, uint8_t> slab;
        std::vector<uint8_t> kept_indices;

        // Allocate 100 objects
        std::vector<uint8_t> all_indices;
        for (int i = 0; i < 100; ++i)
        {
            all_indices.push_back(slab.allocate(i));
        }

        // Deallocate every other one
        for (size_t i = 0; i < all_indices.size(); ++i)
        {
            if (i % 2 == 0)
            {
                slab.deallocate(all_indices[i]);
            }
            else
            {
                kept_indices.push_back(all_indices[i]);
            }
        }

        EXPECT_EQ(slab.size(), 50);

        // Verify kept objects still have correct values
        for (size_t i = 0; i < kept_indices.size(); ++i)
        {
            // These were odd indices in original allocation
            int expected_value = (i * 2) + 1;
            EXPECT_EQ(slab[kept_indices[i]], expected_value);
        }
    }

    // Different Index Types
    TEST(SlabTest, uint8_t_IndexType)
    {
        memory::Slab<int, uint8_t> slab;
        EXPECT_EQ(slab.capacity(), 256);

        auto idx = slab.allocate(42);
        EXPECT_EQ(slab[idx], 42);
    }

    TEST(SlabTest, uint16_t_IndexType)
    {
        memory::Slab<int, uint16_t> slab;
        EXPECT_EQ(slab.capacity(), 65536);

        auto idx = slab.allocate(42);
        EXPECT_EQ(slab[idx], 42);
    }

    // Complex Types
    TEST(SlabTest, ComplexType_Construction)
    {
        memory::Slab<ComplexType, uint8_t> slab;

        auto idx = slab.allocate(100, 200, "test");

        EXPECT_EQ(slab[idx].primary_id, 100);
        EXPECT_EQ(slab[idx].secondary_id, 200);
        EXPECT_EQ(slab[idx].label, "test");
    }

    TEST(SlabTest, ComplexType_MultipleObjects)
    {
        memory::Slab<ComplexType, uint8_t> slab;
        std::vector<uint8_t> indices;

        for (int i = 0; i < 10; ++i)
        {
            auto idx = slab.allocate(i, i * 10, "obj" + std::to_string(i));
            indices.push_back(idx);
        }

        // Verify all objects
        for (size_t i = 0; i < indices.size(); ++i)
        {
            EXPECT_EQ(slab[indices[i]].primary_id, static_cast<int>(i));
            EXPECT_EQ(slab[indices[i]].secondary_id, static_cast<int>(i * 10));
            EXPECT_EQ(slab[indices[i]].label, "obj" + std::to_string(i));
        }
    }

    TEST(SlabTest, ComplexType_Deallocation)
    {
        memory::Slab<ComplexType, uint8_t> slab;

        auto idx = slab.allocate(1, 2, "test_string_that_needs_cleanup");
        EXPECT_EQ(slab.size(), 1);

        slab.deallocate(idx);
        EXPECT_EQ(slab.size(), 0);

        // String should be properly destroyed
    }

    // Memory Tracking Tests
    TEST(SlabTest, TrackedType_NoLeaks_SingleAllocation)
    {
        reset_tracking();

        {
            memory::Slab<TrackedType, uint8_t> slab;
            auto idx = slab.allocate(42);

            EXPECT_EQ(slab[idx].id, 42);
            EXPECT_EQ(constructions.load(), 1);
            EXPECT_EQ(destructions.load(), 0);

            slab.deallocate(idx);

            EXPECT_EQ(destructions.load(), 1);
        }

        EXPECT_FALSE(has_leaks());
    }

    TEST(SlabTest, TrackedType_NoLeaks_MultipleAllocations)
    {
        reset_tracking();

        {
            memory::Slab<TrackedType, uint8_t> slab;
            std::vector<uint8_t> indices;

            for (int i = 0; i < 50; ++i)
            {
                indices.push_back(slab.allocate(i));
            }

            EXPECT_EQ(constructions.load(), 50);

            for (auto idx : indices)
            {
                slab.deallocate(idx);
            }

            EXPECT_EQ(destructions.load(), 50);
        }

        EXPECT_FALSE(has_leaks());
    }

    TEST(SlabTest, TrackedType_NoLeaks_WithoutExplicitDeallocation)
    {
        reset_tracking();

        {
            memory::Slab<TrackedType, uint8_t> slab;

            // Allocate but don't deallocate - destructor should clean up
            for (int i = 0; i < 20; ++i)
            {
                slab.allocate(i);
            }

            EXPECT_EQ(constructions.load(), 20);
            EXPECT_EQ(destructions.load(), 0);

            // Slab destructor runs here
        }

        // All objects should be destroyed by slab destructor
        EXPECT_FALSE(has_leaks());
        EXPECT_EQ(destructions.load(), 20);
    }

    TEST(SlabTest, TrackedType_PartialDeallocation)
    {
        reset_tracking();

        {
            memory::Slab<TrackedType, uint8_t> slab;
            std::vector<uint8_t> indices;

            // Allocate 30 objects
            for (int i = 0; i < 30; ++i)
            {
                indices.push_back(slab.allocate(i));
            }

            EXPECT_EQ(constructions.load(), 30);

            // Deallocate 20 of them
            for (int i = 0; i < 20; ++i)
            {
                slab.deallocate(indices[i]);
            }

            EXPECT_EQ(destructions.load(), 20);

            // Slab destructor should clean up remaining 10
        }

        EXPECT_FALSE(has_leaks());
        EXPECT_EQ(destructions.load(), 30);
    }

    // Stack vs Heap Storage Tests
    TEST(SlabTest, SmallType_UsesStackStorage)
    {
        // Small type, small capacity - should use stack
        using SmallSlab = memory::Slab<int, uint8_t, false, 64 * 1024>;

        SmallSlab slab;
        auto idx = slab.allocate(42);
        EXPECT_EQ(slab[idx], 42);

        // Can't directly test stack vs heap, but we verify it compiles and works
    }

    TEST(SlabTest, LargeType_UsesHeapStorage)
    {
        // Force heap storage
        using HeapSlab = memory::Slab<ComplexType, uint8_t, true>;

        HeapSlab slab;
        auto idx = slab.allocate(1, 2, "heap");

        EXPECT_EQ(slab[idx].primary_id, 1);
        EXPECT_EQ(slab[idx].label, "heap");
    }

    // Perfect Forwarding Tests
    TEST(SlabTest, PerfectForwarding_RvalueString)
    {
        memory::Slab<ComplexType, uint8_t> slab;

        std::string temp = "temporary_string";
        auto idx = slab.allocate(1, 2, std::move(temp));

        EXPECT_EQ(slab[idx].label, "temporary_string");
        // temp should be moved from
    }

    TEST(SlabTest, PerfectForwarding_MultipleArgs)
    {
        memory::Slab<ComplexType, uint8_t> slab;

        int a = 10;
        int b = 20;
        auto idx = slab.allocate(a, b, "test");

        EXPECT_EQ(slab[idx].primary_id, 10);
        EXPECT_EQ(slab[idx].secondary_id, 20);
    }

    // Const Correctness Tests
    TEST(SlabTest, ConstAccessor)
    {
        memory::Slab<int, uint8_t> slab;
        auto idx = slab.allocate(42);

        const auto &const_slab = slab;
        EXPECT_EQ(const_slab[idx], 42);
    }

    TEST(SlabTest, ConstMethods)
    {
        memory::Slab<int, uint8_t> slab;
        slab.allocate(1);
        slab.allocate(2);

        const auto &const_slab = slab;

        EXPECT_EQ(const_slab.capacity(), 256);
        EXPECT_EQ(const_slab.size(), 2);
        EXPECT_TRUE(const_slab.has_free());
    }

    // Stress Tests
    TEST(SlabTest, StressTest_RandomAllocationsAndDeallocations)
    {
        memory::Slab<int, uint8_t> slab;
        std::vector<uint8_t> allocated_indices;

        // Perform 1000 random operations
        for (int i = 0; i < 1000; ++i)
        {
            if (allocated_indices.empty() || (rand() % 2 == 0 && slab.has_free()))
            {
                // Allocate
                auto idx = slab.allocate(i);
                allocated_indices.push_back(idx);
            }
            else
            {
                // Deallocate random index
                size_t rand_idx = rand() % allocated_indices.size();
                slab.deallocate(allocated_indices[rand_idx]);
                allocated_indices.erase(allocated_indices.begin() + rand_idx);
            }
        }

        // Verify size matches
        EXPECT_EQ(slab.size(), allocated_indices.size());

        // Clean up remaining
        for (auto idx : allocated_indices)
        {
            slab.deallocate(idx);
        }

        EXPECT_EQ(slab.size(), 0);
    }

    TEST(SlabTest, StressTest_FillAndEmptyMultipleTimes)
    {
        memory::Slab<int, uint8_t> slab;

        for (int iteration = 0; iteration < 10; ++iteration)
        {
            std::vector<uint8_t> indices;

            // Fill completely
            for (int i = 0; i < 256; ++i)
            {
                indices.push_back(slab.allocate(i + iteration * 1000));
            }

            EXPECT_EQ(slab.size(), 256);
            EXPECT_FALSE(slab.has_free());

            // Verify values
            for (size_t i = 0; i < indices.size(); ++i)
            {
                EXPECT_EQ(slab[indices[i]], static_cast<int>(i + iteration * 1000));
            }

            // Empty completely
            for (auto idx : indices)
            {
                slab.deallocate(idx);
            }

            EXPECT_EQ(slab.size(), 0);
            EXPECT_TRUE(slab.has_free());
        }
    }

    // Index Boundaries
    TEST(SlabTest, AllIndicesAreValid)
    {
        memory::Slab<int, uint8_t> slab;
        std::vector<uint8_t> indices;

        // Allocate all slots and collect indices
        for (int i = 0; i < 256; ++i)
        {
            indices.push_back(slab.allocate(i));
        }

        // Verify all indices are in valid range
        std::vector<bool> seen(256, false);
        for (auto idx : indices)
        {
            EXPECT_LT(idx, 256);
            EXPECT_FALSE(seen[idx]) << "Duplicate index: " << static_cast<int>(idx);
            seen[idx] = true;
        }

        // All indices should have been used exactly once
        EXPECT_TRUE(std::all_of(seen.begin(), seen.end(), [](bool b)
                                { return b; }));
    }

} // namespace mcds::test
