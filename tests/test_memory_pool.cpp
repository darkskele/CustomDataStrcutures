#include <gtest/gtest.h>
#include <type_traits>

#include "mcds/memory/memory_pool.hpp"
#include "test_types/custom_types.hpp"

namespace mcds::tests
{

    using namespace memory;

    // Verify compile time type derivation
    TEST(MemoryPoolTypeTests, IndexTypeSizing)
    {
        // Small capacity uses uint8_t for everything
        {
            using Pool = memory_pool<int, 100>;
            using Layout = compute_pool_layout<100>;

            // BITS_AVAILABLE=8, SLAB_BITS=4, SLOT_BITS=4
            // MAX_SLOT_CAPACITY=16
            // Optimal: 10 slabs × 10 slots = 100 (waste=0) ✓ PERFECT!
            EXPECT_EQ(Layout::NUM_SLABS, 10);
            EXPECT_EQ(Layout::SLAB_CAPACITY, 10);
            EXPECT_EQ(Layout::ACTUAL_CAPACITY, 100);

            EXPECT_TRUE((std::is_same_v<Pool::slab_index_type, uint8_t>));
            EXPECT_TRUE((std::is_same_v<Pool::slot_index_type, uint8_t>));
            EXPECT_TRUE((std::is_same_v<Pool::index_type, uint8_t>));
            EXPECT_EQ(sizeof(Pool::index_type), 1);
        }

        // Medium capacity uses uint16_t
        {
            using Pool = memory_pool<int, 1000>;
            using Layout = compute_pool_layout<1000>;

            // BITS_AVAILABLE=16, SLAB_BITS=4, SLOT_BITS=12
            // MAX_SLOT_CAPACITY=4096
            // Optimal: 1 slab × 1000 slots = 1000 (waste=0) ✓ PERFECT!
            EXPECT_EQ(Layout::NUM_SLABS, 1);
            EXPECT_EQ(Layout::SLAB_CAPACITY, 1000);
            EXPECT_EQ(Layout::ACTUAL_CAPACITY, 1000);

            EXPECT_TRUE((std::is_same_v<Pool::slab_index_type, uint8_t>));
            EXPECT_TRUE((std::is_same_v<Pool::slot_index_type, uint16_t>)); // 1000 needs uint16_t
            EXPECT_TRUE((std::is_same_v<Pool::index_type, uint16_t>));
            EXPECT_EQ(sizeof(Pool::index_type), 2);
        }

        // Large capacity needs uint32_t
        {
            using Pool = memory_pool<int, 100000>;
            using Layout = compute_pool_layout<100000>;

            // BITS_AVAILABLE=32, SLAB_BITS=4, SLOT_BITS=28
            // MAX_SLOT_CAPACITY=268435456
            // Optimal: 1 slab × 100000 slots = 100000 (waste=0) ✓ PERFECT!
            EXPECT_EQ(Layout::NUM_SLABS, 1);
            EXPECT_EQ(Layout::SLAB_CAPACITY, 100000);
            EXPECT_EQ(Layout::ACTUAL_CAPACITY, 100000);

            EXPECT_TRUE((std::is_same_v<Pool::slab_index_type, uint8_t>));
            EXPECT_TRUE((std::is_same_v<Pool::slot_index_type, uint32_t>)); // 100000 needs uint32_t
            EXPECT_TRUE((std::is_same_v<Pool::index_type, uint32_t>));
            EXPECT_EQ(sizeof(Pool::index_type), 4);
        }
    }

    TEST(MemoryPoolTypeTests, SlabAndSlotTypeSizing)
    {
        using Pool = memory_pool<int, 1000>;

        // For CAPACITY=1000  BITS_NEEDED=10, split 5/5
        // NUM_SLABS=32, SLAB_CAPACITY=32
        EXPECT_TRUE((std::is_same_v<Pool::slab_index_type, uint8_t>));
        EXPECT_TRUE((std::is_same_v<Pool::slot_index_type, uint16_t>));
    }

    TEST(MemoryPoolTypeTests, ActualCapacityComputation)
    {
        // Should find exact
        {
            auto pool = memory_pool<int, 100>();
            EXPECT_GE(pool.capacity(), 100);
            EXPECT_EQ(pool.capacity(), 100);
        }

        {
            auto pool = memory_pool<int, 1000>();
            EXPECT_GE(pool.capacity(), 1000);
            EXPECT_EQ(pool.capacity(), 1000);
        }
    }

    // Basic Functionality Tests
    template <typename T>
    class MemoryPoolBasicTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            reset_tracking();
        }

        void TearDown() override
        {
            if constexpr (std::is_same_v<T, TrackedType>)
            {
                EXPECT_FALSE(has_leaks()) << "Leaked " << leaked_objects() << " objects";
            }
        }
    };

    using TestTypes = ::testing::Types<int, ComplexType, TrackedType>;
    TYPED_TEST_SUITE(MemoryPoolBasicTest, TestTypes);

    TYPED_TEST(MemoryPoolBasicTest, DefaultConstruction)
    {
        memory_pool<TypeParam, 10> pool;

        EXPECT_TRUE(pool.has_free());
        EXPECT_EQ(pool.size(), 0);
        EXPECT_GE(pool.capacity(), 10);
    }

    TYPED_TEST(MemoryPoolBasicTest, SingleAllocation)
    {
        memory_pool<TypeParam, 10> pool;

        auto idx = pool.allocate();

        EXPECT_EQ(pool.size(), 1);
        EXPECT_TRUE(pool.has_free());
    }

    TYPED_TEST(MemoryPoolBasicTest, AllocationWithArguments)
    {
        memory_pool<ComplexType, 10> pool;

        auto idx = pool.allocate(42, 99, "test");

        const auto &obj = pool[idx];
        EXPECT_EQ(obj.primary_id, 42);
        EXPECT_EQ(obj.secondary_id, 99);
        EXPECT_EQ(obj.label, "test");
    }

    TYPED_TEST(MemoryPoolBasicTest, MultipleAllocations)
    {
        memory_pool<TypeParam, 10> pool;

        std::vector<typename decltype(pool)::index_type> indices;
        for (int i = 0; i < 5; ++i)
        {
            indices.push_back(pool.allocate());
        }

        EXPECT_EQ(pool.size(), 5);
        EXPECT_TRUE(pool.has_free());
        EXPECT_EQ(indices.size(), 5);
    }

    TYPED_TEST(MemoryPoolBasicTest, FillToCapacity)
    {
        memory_pool<TypeParam, 10> pool;
        const size_t capacity = pool.capacity();

        std::vector<typename decltype(pool)::index_type> indices;
        for (size_t i = 0; i < capacity; ++i)
        {
            EXPECT_TRUE(pool.has_free()) << "Pool should have space at iteration " << i;
            indices.push_back(pool.allocate());
        }

        EXPECT_EQ(pool.size(), capacity);
        EXPECT_FALSE(pool.has_free());
    }

    TYPED_TEST(MemoryPoolBasicTest, SingleDeallocation)
    {
        memory_pool<TypeParam, 10> pool;

        auto idx = pool.allocate();
        EXPECT_EQ(pool.size(), 1);

        pool.deallocate(idx);
        EXPECT_EQ(pool.size(), 0);
        EXPECT_TRUE(pool.has_free());
    }

    TYPED_TEST(MemoryPoolBasicTest, AllocationDeallocationCycle)
    {
        memory_pool<TypeParam, 10> pool;

        // Allocate
        auto idx1 = pool.allocate();
        auto idx2 = pool.allocate();
        EXPECT_EQ(pool.size(), 2);

        // Deallocate first
        pool.deallocate(idx1);
        EXPECT_EQ(pool.size(), 1);

        // Allocate again (should reuse slot)
        auto idx3 = pool.allocate();
        EXPECT_EQ(pool.size(), 2);

        // Deallocate all
        pool.deallocate(idx2);
        pool.deallocate(idx3);
        EXPECT_EQ(pool.size(), 0);
    }

    // Access Tests
    TEST(MemoryPoolAccessTest, MutableAccess)
    {
        memory_pool<ComplexType, 10> pool;

        auto idx = pool.allocate(1, 2, "original");

        // Modify through operator[]
        pool[idx].primary_id = 99;
        pool[idx].label = "modified";

        EXPECT_EQ(pool[idx].primary_id, 99);
        EXPECT_EQ(pool[idx].secondary_id, 2);
        EXPECT_EQ(pool[idx].label, "modified");
    }

    TEST(MemoryPoolAccessTest, ConstAccess)
    {
        memory_pool<ComplexType, 10> pool;
        auto idx = pool.allocate(5, 10, "test");

        const auto &const_pool = pool;
        const auto &obj = const_pool[idx];

        EXPECT_EQ(obj.primary_id, 5);
        EXPECT_EQ(obj.secondary_id, 10);
        EXPECT_EQ(obj.label, "test");
    }

    TEST(MemoryPoolAccessTest, MultipleIndependentObjects)
    {
        memory_pool<ComplexType, 10> pool;

        auto idx1 = pool.allocate(1, 10, "first");
        auto idx2 = pool.allocate(2, 20, "second");
        auto idx3 = pool.allocate(3, 30, "third");

        // Verify all objects maintain independence
        EXPECT_EQ(pool[idx1].primary_id, 1);
        EXPECT_EQ(pool[idx2].primary_id, 2);
        EXPECT_EQ(pool[idx3].primary_id, 3);

        // Modify one
        pool[idx2].label = "modified";

        // Others unchanged
        EXPECT_EQ(pool[idx1].label, "first");
        EXPECT_EQ(pool[idx2].label, "modified");
        EXPECT_EQ(pool[idx3].label, "third");
    }

    // Resource Management Tests
    TEST(MemoryPoolResourceTest, NoLeaksOnDeallocation)
    {
        reset_tracking();

        {
            memory_pool<TrackedType, 10> pool;

            auto idx1 = pool.allocate(1);
            auto idx2 = pool.allocate(2);
            auto idx3 = pool.allocate(3);

            EXPECT_EQ(constructions.load(), 3);
            EXPECT_EQ(destructions.load(), 0);

            pool.deallocate(idx2);
            EXPECT_EQ(destructions.load(), 1);

            pool.deallocate(idx1);
            pool.deallocate(idx3);
            EXPECT_EQ(destructions.load(), 3);
        }

        EXPECT_FALSE(has_leaks());
    }

    TEST(MemoryPoolResourceTest, NoLeaksOnDestruction)
    {
        reset_tracking();

        {
            memory_pool<TrackedType, 10> pool;

            for (int i = 0; i < 5; ++i)
            {
                pool.allocate(i);
            }

            EXPECT_EQ(constructions.load(), 5);
            EXPECT_EQ(destructions.load(), 0);
        }
        // Pool destructor should clean up all remaining objects

        EXPECT_FALSE(has_leaks()) << "Leaked " << leaked_objects() << " objects";
    }

    TEST(MemoryPoolResourceTest, ResourceValuesCorrect)
    {
        reset_tracking();

        memory_pool<TrackedType, 10> pool;

        auto idx1 = pool.allocate(5);
        auto idx2 = pool.allocate(7);

        EXPECT_EQ(pool[idx1].id, 5);
        EXPECT_EQ(pool[idx1].get_resource_value(), 50); // id * 10

        EXPECT_EQ(pool[idx2].id, 7);
        EXPECT_EQ(pool[idx2].get_resource_value(), 70);
    }

    // Fragmentation and Reuse Tests
    TEST(MemoryPoolFragmentationTest, ReusesDeallocatedSlots)
    {
        memory_pool<int, 10> pool;
        const size_t capacity = pool.capacity();

        // Fill pool
        std::vector<typename decltype(pool)::index_type> indices;
        for (size_t i = 0; i < capacity; ++i)
        {
            indices.push_back(pool.allocate());
        }

        EXPECT_FALSE(pool.has_free());

        // Deallocate every other slot
        for (size_t i = 0; i < indices.size(); i += 2)
        {
            pool.deallocate(indices[i]);
        }

        size_t expected_size = (capacity + 1) / 2; // Round up for odd capacity
        EXPECT_EQ(pool.size(), expected_size);
        EXPECT_TRUE(pool.has_free());

        // Should be able to allocate into freed slots
        for (size_t i = 0; i < capacity / 2; ++i)
        {
            EXPECT_TRUE(pool.has_free());
            pool.allocate();
        }

        EXPECT_EQ(pool.size(), capacity);
    }

    TEST(MemoryPoolFragmentationTest, HandlesChurnPattern)
    {
        memory_pool<ComplexType, 200> pool;

        std::vector<typename decltype(pool)::index_type> active;

        // Simulate churn: allocate, use, deallocate pattern
        for (int cycle = 0; cycle < 10; ++cycle)
        {
            // Allocate batch
            for (int i = 0; i < 20; ++i)
            {
                active.push_back(pool.allocate(cycle, i));
            }

            // Deallocate half
            for (int i = 0; i < 10; ++i)
            {
                pool.deallocate(active.back());
                active.pop_back();
            }
        }

        EXPECT_GT(pool.size(), 0);
        EXPECT_LE(pool.size(), pool.capacity());
        EXPECT_TRUE(pool.has_free());
    }

    // Edge Cases
    TEST(MemoryPoolEdgeCaseTest, MinimalCapacity)
    {
        memory_pool<int, 1> pool;

        EXPECT_GE(pool.capacity(), 1);
        EXPECT_TRUE(pool.has_free());

        auto idx = pool.allocate(42);
        EXPECT_EQ(pool[idx], 42);
    }

    TEST(MemoryPoolEdgeCaseTest, PowerOfTwoCapacity)
    {
        // These should have no overcapacity (or minimal)
        {
            memory_pool<int, 2> pool;
            EXPECT_EQ(pool.capacity(), 2);
        }
        {
            memory_pool<int, 4> pool;
            EXPECT_EQ(pool.capacity(), 4);
        }
        {
            memory_pool<int, 16> pool;
            EXPECT_EQ(pool.capacity(), 16);
        }
        {
            memory_pool<int, 256> pool;
            EXPECT_EQ(pool.capacity(), 256);
        }
    }

    TEST(MemoryPoolEdgeCaseTest, LargeCapacity)
    {
        memory_pool<int, 10000> pool;

        EXPECT_GE(pool.capacity(), 10000);
        EXPECT_TRUE(pool.has_free());

        // Allocate a few to verify it works
        for (int i = 0; i < 100; ++i)
        {
            pool.allocate(i);
        }

        EXPECT_EQ(pool.size(), 100);
    }

    // Index Packing/Unpacking Tests
    TEST(MemoryPoolIndexTest, IndexUniqueness)
    {
        memory_pool<int, 100> pool;

        std::set<typename decltype(pool)::index_type> indices;

        // Allocate many objects
        for (int i = 0; i < 50; ++i)
        {
            auto idx = pool.allocate(i);
            EXPECT_TRUE(indices.insert(idx).second) << "Duplicate index: " << idx;
        }

        EXPECT_EQ(indices.size(), 50);
    }

    TEST(MemoryPoolIndexTest, IndexStabilityAfterDeallocation)
    {
        memory_pool<int, 100> pool;

        auto idx1 = pool.allocate(111);
        auto idx2 = pool.allocate(222);
        auto idx3 = pool.allocate(333);

        // Deallocate middle
        pool.deallocate(idx2);

        // Original indices still valid
        EXPECT_EQ(pool[idx1], 111);
        EXPECT_EQ(pool[idx3], 333);

        // New allocation gets new index
        auto idx4 = pool.allocate(444);

        // All indices still work
        EXPECT_EQ(pool[idx1], 111);
        EXPECT_EQ(pool[idx3], 333);
        EXPECT_EQ(pool[idx4], 444);
    }

} // namespace mcds::tests
