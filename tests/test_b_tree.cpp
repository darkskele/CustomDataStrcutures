#include <gtest/gtest.h>

#include "mcds/b_tree.hpp"
#include "test_types/custom_types.hpp"

#include <vector>
#include <algorithm>
#include <random>
#include <set>

namespace mcds::tests
{

    // Type combinations to test
    using TestTypes = ::testing::Types<
        // Primitive key, primitive value
        std::pair<int, int>,
        std::pair<int, double>,
        std::pair<double, int>,

        // Primitive key, complex value
        std::pair<int, test_types::ComplexType>,
        std::pair<int, test_types::TrackedType>,

        // Complex key, primitive value
        std::pair<test_types::ComplexType, int>,
        std::pair<test_types::TrackedType, int>,

        // Complex key, complex value
        std::pair<test_types::ComplexType, test_types::ComplexType>,
        std::pair<test_types::TrackedType, test_types::TrackedType>>;

    // Typed test fixture
    template <typename T>
    class BTreeTypedTest : public ::testing::Test
    {
    protected:
        using KeyType = typename T::first_type;
        using ValueType = typename T::second_type;
        using TreeType = BTree<KeyType, ValueType, 16, 1000>;

        void SetUp() override
        {
            tree = std::make_unique<TreeType>();
        }

        void TearDown() override
        {
            tree.reset();

            // Check for leaks if using TrackedType
            if constexpr (std::is_same_v<KeyType, test_types::TrackedType> ||
                          std::is_same_v<ValueType, test_types::TrackedType>)
            {
                EXPECT_FALSE(test_types::has_leaks())
                    << "Memory leak detected: " << test_types::leaked_objects() << " objects";
                test_types::reset_tracking();
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
            else if constexpr (std::is_same_v<KeyType, test_types::ComplexType>)
            {
                return test_types::ComplexType(value, value * 2, "key_" + std::to_string(value));
            }
            else if constexpr (std::is_same_v<KeyType, test_types::TrackedType>)
            {
                return test_types::TrackedType(value);
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
            else if constexpr (std::is_same_v<ValueType, test_types::ComplexType>)
            {
                return test_types::ComplexType(value * 100, value * 200, "val_" + std::to_string(value));
            }
            else if constexpr (std::is_same_v<ValueType, test_types::TrackedType>)
            {
                return test_types::TrackedType(value * 100);
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
            else if constexpr (std::is_same_v<ValueType, test_types::ComplexType>)
            {
                return val.primary_id == expected * 100;
            }
            else if constexpr (std::is_same_v<ValueType, test_types::TrackedType>)
            {
                return val.id == expected * 100;
            }
        }

        std::unique_ptr<TreeType> tree;
    };

    TYPED_TEST_SUITE(BTreeTypedTest, TestTypes);

    // Basic Operations Tests

    TYPED_TEST(BTreeTypedTest, EmptyTree)
    {
        EXPECT_TRUE(this->tree->empty());
        EXPECT_EQ(this->tree->size(), 0);
        EXPECT_EQ(this->tree->find_min(), nullptr);
        EXPECT_EQ(this->tree->find_max(), nullptr);
    }

    TYPED_TEST(BTreeTypedTest, SingleInsertAndFind)
    {
        auto key = this->make_key(42);
        auto value = this->make_value(42);

        EXPECT_TRUE(this->tree->insert(key, value));
        EXPECT_FALSE(this->tree->empty());
        EXPECT_EQ(this->tree->size(), 1);

        auto *found = this->tree->find(key);
        ASSERT_NE(found, nullptr);
        EXPECT_TRUE(this->verify_value(*found, 42));
    }

    TYPED_TEST(BTreeTypedTest, MultipleInserts)
    {
        const int count = 100;

        for (int i = 0; i < count; ++i)
        {
            EXPECT_TRUE(this->tree->insert(this->make_key(i), this->make_value(i)));
        }

        EXPECT_EQ(this->tree->size(), count);

        for (int i = 0; i < count; ++i)
        {
            auto *found = this->tree->find(this->make_key(i));
            ASSERT_NE(found, nullptr) << "Failed to find key: " << i;
            EXPECT_TRUE(this->verify_value(*found, i));
        }
    }

    TYPED_TEST(BTreeTypedTest, InsertDuplicateUpdates)
    {
        auto key = this->make_key(42);
        auto value1 = this->make_value(42);
        auto value2 = this->make_value(99);

        EXPECT_TRUE(this->tree->insert(key, value1));
        EXPECT_EQ(this->tree->size(), 1);

        EXPECT_TRUE(this->tree->insert(key, value2));
        EXPECT_EQ(this->tree->size(), 1); // Size shouldn't change

        auto *found = this->tree->find(key);
        ASSERT_NE(found, nullptr);
        EXPECT_TRUE(this->verify_value(*found, 99)); // Should have updated value
    }

    TYPED_TEST(BTreeTypedTest, InsertReverseOrder)
    {
        const int count = 100;

        for (int i = count - 1; i >= 0; --i)
        {
            EXPECT_TRUE(this->tree->insert(this->make_key(i), this->make_value(i)));
        }

        EXPECT_EQ(this->tree->size(), count);

        for (int i = 0; i < count; ++i)
        {
            auto *found = this->tree->find(this->make_key(i));
            ASSERT_NE(found, nullptr);
            EXPECT_TRUE(this->verify_value(*found, i));
        }
    }

    TYPED_TEST(BTreeTypedTest, InsertRandomOrder)
    {
        const int count = 100;
        std::vector<int> values(count);
        std::iota(values.begin(), values.end(), 0);

        std::random_device rd;
        std::mt19937 gen(42); // Fixed seed for reproducibility
        std::shuffle(values.begin(), values.end(), gen);

        for (int val : values)
        {
            EXPECT_TRUE(this->tree->insert(this->make_key(val), this->make_value(val)));
        }

        EXPECT_EQ(this->tree->size(), count);

        for (int i = 0; i < count; ++i)
        {
            auto *found = this->tree->find(this->make_key(i));
            ASSERT_NE(found, nullptr);
            EXPECT_TRUE(this->verify_value(*found, i));
        }
    }

    TYPED_TEST(BTreeTypedTest, FindNonExistent)
    {
        this->tree->insert(this->make_key(10), this->make_value(10));
        this->tree->insert(this->make_key(20), this->make_value(20));
        this->tree->insert(this->make_key(30), this->make_value(30));

        EXPECT_EQ(this->tree->find(this->make_key(5)), nullptr);
        EXPECT_EQ(this->tree->find(this->make_key(15)), nullptr);
        EXPECT_EQ(this->tree->find(this->make_key(25)), nullptr);
        EXPECT_EQ(this->tree->find(this->make_key(35)), nullptr);
    }

    // Deletion Tests
    TYPED_TEST(BTreeTypedTest, DeleteSingle)
    {
        auto key = this->make_key(42);
        this->tree->insert(key, this->make_value(42));

        EXPECT_TRUE(this->tree->erase(key));
        EXPECT_TRUE(this->tree->empty());
        EXPECT_EQ(this->tree->size(), 0);
        EXPECT_EQ(this->tree->find(key), nullptr);
    }

    TYPED_TEST(BTreeTypedTest, DeleteNonExistent)
    {
        this->tree->insert(this->make_key(10), this->make_value(10));

        EXPECT_FALSE(this->tree->erase(this->make_key(20)));
        EXPECT_EQ(this->tree->size(), 1);
    }

    TYPED_TEST(BTreeTypedTest, DeleteFromLeaf)
    {
        for (int i = 0; i < 50; ++i)
        {
            this->tree->insert(this->make_key(i), this->make_value(i));
        }

        EXPECT_TRUE(this->tree->erase(this->make_key(25)));
        EXPECT_EQ(this->tree->size(), 49);
        EXPECT_EQ(this->tree->find(this->make_key(25)), nullptr);

        // Verify other keys still exist
        for (int i = 0; i < 50; ++i)
        {
            if (i != 25)
            {
                EXPECT_NE(this->tree->find(this->make_key(i)), nullptr);
            }
        }
    }

    TYPED_TEST(BTreeTypedTest, DeleteMultiple)
    {
        const int count = 100;

        for (int i = 0; i < count; ++i)
        {
            EXPECT_TRUE(this->tree->insert(this->make_key(i), this->make_value(i))) << "Failed to insert on key " << i << std::endl;
        }

        // Delete every other element
        for (int i = 0; i < count; i += 2)
        {
            EXPECT_TRUE(this->tree->erase(this->make_key(i))) << "Failed erase on key " << i << std::endl;
        }

        EXPECT_EQ(this->tree->size(), count / 2);

        // Verify deleted keys are gone
        for (int i = 0; i < count; i += 2)
        {
            EXPECT_EQ(this->tree->find(this->make_key(i)), nullptr);
        }

        // Verify remaining keys exist
        for (int i = 1; i < count; i += 2)
        {
            EXPECT_NE(this->tree->find(this->make_key(i)), nullptr);
        }
    }

    TYPED_TEST(BTreeTypedTest, DeleteAll)
    {
        const int count = 100;

        for (int i = 0; i < count; ++i)
        {
            EXPECT_TRUE(this->tree->insert(this->make_key(i), this->make_value(i)));
        }

        for (int i = 0; i < count; ++i)
        {
            EXPECT_TRUE(this->tree->erase(this->make_key(i)));
        }

        EXPECT_TRUE(this->tree->empty());
        EXPECT_EQ(this->tree->size(), 0);
    }

    TYPED_TEST(BTreeTypedTest, DeleteAndReinsert)
    {
        auto key = this->make_key(42);
        auto value1 = this->make_value(42);
        auto value2 = this->make_value(99);

        this->tree->insert(key, value1);
        EXPECT_TRUE(this->tree->erase(key));
        EXPECT_EQ(this->tree->find(key), nullptr);

        this->tree->insert(key, value2);
        auto *found = this->tree->find(key);
        ASSERT_NE(found, nullptr);
        EXPECT_TRUE(this->verify_value(*found, 99));
    }

    // Min/Max Tests
    TYPED_TEST(BTreeTypedTest, FindMinMax)
    {
        this->tree->insert(this->make_key(50), this->make_value(50));
        this->tree->insert(this->make_key(25), this->make_value(25));
        this->tree->insert(this->make_key(75), this->make_value(75));
        this->tree->insert(this->make_key(10), this->make_value(10));
        this->tree->insert(this->make_key(90), this->make_value(90));

        auto *min_val = this->tree->find_min();
        auto *max_val = this->tree->find_max();

        ASSERT_NE(min_val, nullptr);
        ASSERT_NE(max_val, nullptr);
        EXPECT_TRUE(this->verify_value(*min_val, 10));
        EXPECT_TRUE(this->verify_value(*max_val, 90));
    }

    TYPED_TEST(BTreeTypedTest, FindMinMaxAfterDeletions)
    {
        for (int i = 0; i < 100; ++i)
        {
            this->tree->insert(this->make_key(i), this->make_value(i));
        }

        this->tree->erase(this->make_key(0));
        this->tree->erase(this->make_key(99));

        auto *min_val = this->tree->find_min();
        auto *max_val = this->tree->find_max();

        ASSERT_NE(min_val, nullptr);
        ASSERT_NE(max_val, nullptr);
        EXPECT_TRUE(this->verify_value(*min_val, 1));
        EXPECT_TRUE(this->verify_value(*max_val, 98));
    }

    // Lower/Upper Bound Tests
    TYPED_TEST(BTreeTypedTest, LowerBoundExactMatch)
    {
        for (int i = 0; i < 100; i += 10)
        {
            this->tree->insert(this->make_key(i), this->make_value(i));
        }

        auto *result = this->tree->lower_bound(this->make_key(50));
        ASSERT_NE(result, nullptr);
        EXPECT_TRUE(this->verify_value(*result, 50));
    }

    TYPED_TEST(BTreeTypedTest, LowerBoundBetweenKeys)
    {
        for (int i = 0; i < 100; i += 10)
        {
            this->tree->insert(this->make_key(i), this->make_value(i));
        }

        auto *result = this->tree->lower_bound(this->make_key(55));
        ASSERT_NE(result, nullptr);
        EXPECT_TRUE(this->verify_value(*result, 60));
    }

    TYPED_TEST(BTreeTypedTest, LowerBoundNotFound)
    {
        for (int i = 0; i < 100; i += 10)
        {
            this->tree->insert(this->make_key(i), this->make_value(i));
        }

        auto *result = this->tree->lower_bound(this->make_key(100));
        EXPECT_EQ(result, nullptr);
    }

    TYPED_TEST(BTreeTypedTest, UpperBoundExactMatch)
    {
        for (int i = 0; i < 100; i += 10)
        {
            this->tree->insert(this->make_key(i), this->make_value(i));
        }

        auto *result = this->tree->upper_bound(this->make_key(50));
        ASSERT_NE(result, nullptr);
        EXPECT_TRUE(this->verify_value(*result, 60));
    }

    TYPED_TEST(BTreeTypedTest, UpperBoundBetweenKeys)
    {
        for (int i = 0; i < 100; i += 10)
        {
            this->tree->insert(this->make_key(i), this->make_value(i));
        }

        auto *result = this->tree->upper_bound(this->make_key(55));
        ASSERT_NE(result, nullptr);
        EXPECT_TRUE(this->verify_value(*result, 60));
    }

    TYPED_TEST(BTreeTypedTest, UpperBoundNotFound)
    {
        for (int i = 0; i < 100; i += 10)
        {
            this->tree->insert(this->make_key(i), this->make_value(i));
        }

        auto *result = this->tree->upper_bound(this->make_key(90));
        EXPECT_EQ(result, nullptr);
    }

    // Operator[] Tests
    TYPED_TEST(BTreeTypedTest, SubscriptOperatorInsert)
    {
        auto key = this->make_key(42);
        auto &value = (*this->tree)[key];

        // Value should be default-constructed
        EXPECT_EQ(this->tree->size(), 1);
    }

    TYPED_TEST(BTreeTypedTest, SubscriptOperatorAccess)
    {
        auto key = this->make_key(42);
        this->tree->insert(key, this->make_value(42));

        auto &value = (*this->tree)[key];
        EXPECT_TRUE(this->verify_value(value, 42));
    }

    // Stress Tests
    TYPED_TEST(BTreeTypedTest, StressTestLargeInsertion)
    {
        const int count = 1000;

        for (int i = 0; i < count; ++i)
        {
            this->tree->insert(this->make_key(i), this->make_value(i));
        }

        EXPECT_EQ(this->tree->size(), count);

        // Spot check
        for (int i = 0; i < count; i += 10)
        {
            EXPECT_NE(this->tree->find(this->make_key(i)), nullptr);
        }
    }

    TYPED_TEST(BTreeTypedTest, StressTestMixedOperations)
    {
        std::random_device rd;
        std::mt19937 gen(42);
        std::uniform_int_distribution<> dis(0, 999);
        std::set<int> inserted_keys;

        // Perform random operations
        for (int op = 0; op < 500; ++op)
        {
            int key_int = dis(gen);
            int operation = dis(gen) % 3;

            if (operation == 0) // Insert
            {
                EXPECT_TRUE(this->tree->insert(this->make_key(key_int), this->make_value(key_int))) << "Operation =" + std::to_string(op);
                inserted_keys.insert(key_int);
            }
            else if (operation == 1) // Delete
            {
                bool should_exist = inserted_keys.count(key_int) > 0;
                EXPECT_EQ(this->tree->erase(this->make_key(key_int)), should_exist) << "Operation =" + std::to_string(op);
                inserted_keys.erase(key_int);
            }
            else // Find
            {
                auto *result = this->tree->find(this->make_key(key_int));
                bool should_exist = inserted_keys.count(key_int) > 0;
                EXPECT_EQ(result != nullptr, should_exist) << "Operation =" + std::to_string(op);
            }
        }

        // Verify final state
        EXPECT_EQ(this->tree->size(), inserted_keys.size());
    }

    // Edge Cases
    TYPED_TEST(BTreeTypedTest, RootSplit)
    {
        // Insert enough to cause root split
        const int keys_to_insert = 30;

        for (int i = 0; i < keys_to_insert; ++i)
        {
            this->tree->insert(this->make_key(i), this->make_value(i));
        }

        // Verify all keys are still accessible
        for (int i = 0; i < keys_to_insert; ++i)
        {
            EXPECT_NE(this->tree->find(this->make_key(i)), nullptr);
        }
    }

    TYPED_TEST(BTreeTypedTest, NodeMerge)
    {
        // Insert and then delete to trigger merges
        for (int i = 0; i < 50; ++i)
        {
            this->tree->insert(this->make_key(i), this->make_value(i));
        }

        // Delete in a pattern that should trigger merges
        for (int i = 0; i < 40; ++i)
        {
            EXPECT_TRUE(this->tree->erase(this->make_key(i)));
        }

        // Verify remaining keys
        for (int i = 40; i < 50; ++i)
        {
            EXPECT_NE(this->tree->find(this->make_key(i)), nullptr);
        }
    }

    TYPED_TEST(BTreeTypedTest, Redistribution)
    {
        // Pattern designed to trigger redistribution
        for (int i = 0; i < 100; ++i)
        {
            this->tree->insert(this->make_key(i), this->make_value(i));
        }

        // Delete keys to trigger redistribution
        for (int i = 30; i < 70; ++i)
        {
            this->tree->erase(this->make_key(i));
        }

        // Verify structure integrity
        for (int i = 0; i < 30; ++i)
        {
            EXPECT_NE(this->tree->find(this->make_key(i)), nullptr);
        }
        for (int i = 70; i < 100; ++i)
        {
            EXPECT_NE(this->tree->find(this->make_key(i)), nullptr);
        }
    }

    // Borrowing Tests
    TYPED_TEST(BTreeTypedTest, BorrowFromLeftSibling)
    {
        // Create a situation where borrowing from left is needed
        // Insert enough to create multiple nodes
        for (int i = 0; i < 50; ++i)
        {
            this->tree->insert(this->make_key(i), this->make_value(i));
        }

        // Delete to create a node with MIN_KEYS, with left sibling having extras
        // This pattern should trigger borrow from left
        for (int i = 15; i < 25; ++i)
        {
            this->tree->erase(this->make_key(i));
        }

        // Verify structure is still valid
        EXPECT_EQ(this->tree->size(), 40);

        // Verify remaining keys are accessible
        for (int i = 0; i < 50; ++i)
        {
            if (i < 15 || i >= 25)
            {
                EXPECT_NE(this->tree->find(this->make_key(i)), nullptr)
                    << "Key " << i << " should exist";
            }
            else
            {
                EXPECT_EQ(this->tree->find(this->make_key(i)), nullptr)
                    << "Key " << i << " should not exist";
            }
        }
    }

    TYPED_TEST(BTreeTypedTest, BorrowFromRightSibling)
    {
        // Create situation where borrowing from right is needed
        for (int i = 0; i < 50; ++i)
        {
            this->tree->insert(this->make_key(i), this->make_value(i));
        }

        // Delete from left side to trigger borrow from right
        for (int i = 0; i < 10; ++i)
        {
            this->tree->erase(this->make_key(i));
        }

        EXPECT_EQ(this->tree->size(), 40);

        for (int i = 10; i < 50; ++i)
        {
            EXPECT_NE(this->tree->find(this->make_key(i)), nullptr);
        }
    }

    // Lower/Upper Bound Edge Cases
    TYPED_TEST(BTreeTypedTest, LowerBoundOnEmptyTree)
    {
        EXPECT_EQ(this->tree->lower_bound(this->make_key(10)), nullptr);
    }

    TYPED_TEST(BTreeTypedTest, LowerBoundSmallerThanAll)
    {
        for (int i = 10; i < 100; i += 10)
        {
            this->tree->insert(this->make_key(i), this->make_value(i));
        }

        auto *result = this->tree->lower_bound(this->make_key(5));
        ASSERT_NE(result, nullptr);
        EXPECT_TRUE(this->verify_value(*result, 10));
    }

    TYPED_TEST(BTreeTypedTest, UpperBoundLargerThanAll)
    {
        for (int i = 0; i < 100; i += 10)
        {
            this->tree->insert(this->make_key(i), this->make_value(i));
        }

        auto *result = this->tree->upper_bound(this->make_key(100));
        EXPECT_EQ(result, nullptr);
    }

    TYPED_TEST(BTreeTypedTest, BoundsOnSingleElement)
    {
        this->tree->insert(this->make_key(50), this->make_value(50));

        // Lower bound on exact match
        auto *lb_exact = this->tree->lower_bound(this->make_key(50));
        ASSERT_NE(lb_exact, nullptr);
        EXPECT_TRUE(this->verify_value(*lb_exact, 50));

        // Lower bound smaller
        auto *lb_smaller = this->tree->lower_bound(this->make_key(40));
        ASSERT_NE(lb_smaller, nullptr);
        EXPECT_TRUE(this->verify_value(*lb_smaller, 50));

        // Lower bound larger
        auto *lb_larger = this->tree->lower_bound(this->make_key(60));
        EXPECT_EQ(lb_larger, nullptr);

        // Upper bound
        auto *ub = this->tree->upper_bound(this->make_key(50));
        EXPECT_EQ(ub, nullptr);

        auto *ub_smaller = this->tree->upper_bound(this->make_key(40));
        ASSERT_NE(ub_smaller, nullptr);
        EXPECT_TRUE(this->verify_value(*ub_smaller, 50));
    }

    TYPED_TEST(BTreeTypedTest, BoundsWithGaps)
    {
        // Insert with gaps
        this->tree->insert(this->make_key(10), this->make_value(10));
        this->tree->insert(this->make_key(20), this->make_value(20));
        this->tree->insert(this->make_key(40), this->make_value(40));
        this->tree->insert(this->make_key(50), this->make_value(50));

        // Test in gaps
        auto *lb_15 = this->tree->lower_bound(this->make_key(15));
        ASSERT_NE(lb_15, nullptr);
        EXPECT_TRUE(this->verify_value(*lb_15, 20));

        auto *lb_35 = this->tree->lower_bound(this->make_key(35));
        ASSERT_NE(lb_35, nullptr);
        EXPECT_TRUE(this->verify_value(*lb_35, 40));

        auto *ub_35 = this->tree->upper_bound(this->make_key(35));
        ASSERT_NE(ub_35, nullptr);
        EXPECT_TRUE(this->verify_value(*ub_35, 40));
    }

    // Internal Node Deletion Paths
    TYPED_TEST(BTreeTypedTest, DeleteFromInternalUsePredecessor)
    {
        // Build tree tall enough to have internal nodes
        for (int i = 0; i < 100; ++i)
        {
            this->tree->insert(this->make_key(i), this->make_value(i));
        }

        // Delete a key that should be in an internal node
        // After many insertions, middle keys often end up in internal nodes
        EXPECT_TRUE(this->tree->erase(this->make_key(50)));

        // Verify it's gone and others remain
        EXPECT_EQ(this->tree->find(this->make_key(50)), nullptr);
        EXPECT_NE(this->tree->find(this->make_key(49)), nullptr);
        EXPECT_NE(this->tree->find(this->make_key(51)), nullptr);
    }

    TYPED_TEST(BTreeTypedTest, DeleteFromInternalUseSuccessor)
    {
        // Similar to above but structured to favor successor path
        for (int i = 100; i >= 0; --i)
        {
            this->tree->insert(this->make_key(i), this->make_value(i));
        }

        EXPECT_TRUE(this->tree->erase(this->make_key(50)));
        EXPECT_EQ(this->tree->find(this->make_key(50)), nullptr);
    }

    TYPED_TEST(BTreeTypedTest, DeleteFromInternalCausingMerge)
    {
        // Insert enough for internal nodes
        for (int i = 0; i < 100; ++i)
        {
            this->tree->insert(this->make_key(i), this->make_value(i));
        }

        // Delete many keys to reduce tree and force merges
        for (int i = 0; i < 80; ++i)
        {
            this->tree->erase(this->make_key(i));
        }

        EXPECT_EQ(this->tree->size(), 20);

        // Verify remaining keys
        for (int i = 80; i < 100; ++i)
        {
            EXPECT_NE(this->tree->find(this->make_key(i)), nullptr);
        }
    }

    // Root Special Cases
    TYPED_TEST(BTreeTypedTest, RootReplacementAfterDeletion)
    {
        // Insert enough to create child nodes
        for (int i = 0; i < 30; ++i)
        {
            this->tree->insert(this->make_key(i), this->make_value(i));
        }

        size_t size_before = this->tree->size();

        // Delete all but one key from root level, should cause root replacement
        for (int i = 0; i < 29; ++i)
        {
            this->tree->erase(this->make_key(i));
        }

        EXPECT_EQ(this->tree->size(), 1);
        EXPECT_NE(this->tree->find(this->make_key(29)), nullptr);

        // Verify can still insert after root replacement
        this->tree->insert(this->make_key(100), this->make_value(100));
        EXPECT_EQ(this->tree->size(), 2);
    }

    TYPED_TEST(BTreeTypedTest, MultipleRootReplacements)
    {
        // Build up
        for (int i = 0; i < 50; ++i)
        {
            this->tree->insert(this->make_key(i), this->make_value(i));
        }

        // Tear down to almost empty, causing multiple root changes
        for (int i = 0; i < 48; ++i)
        {
            this->tree->erase(this->make_key(i));
        }

        EXPECT_EQ(this->tree->size(), 2);

        // Build back up
        for (int i = 0; i < 50; ++i)
        {
            this->tree->insert(this->make_key(i + 100), this->make_value(i + 100));
        }

        EXPECT_EQ(this->tree->size(), 52);
    }

    // Value Modification
    TYPED_TEST(BTreeTypedTest, ModifyValueThroughPointer)
    {
        auto key = this->make_key(42);
        this->tree->insert(key, this->make_value(42));

        auto *val = this->tree->find(key);
        ASSERT_NE(val, nullptr);

        // Modify through pointer
        *val = this->make_value(99);

        // Verify modification persisted
        auto *val2 = this->tree->find(key);
        ASSERT_NE(val2, nullptr);
        EXPECT_TRUE(this->verify_value(*val2, 99));
    }

    TYPED_TEST(BTreeTypedTest, ModifyValueThroughSubscript)
    {
        auto key = this->make_key(42);
        (*this->tree)[key] = this->make_value(42);

        // Modify
        (*this->tree)[key] = this->make_value(99);

        // Verify
        EXPECT_TRUE(this->verify_value((*this->tree)[key], 99));
    }

    // Const Correctness
    TYPED_TEST(BTreeTypedTest, ConstMinMax)
    {
        for (int i = 0; i < 50; ++i)
        {
            this->tree->insert(this->make_key(i), this->make_value(i));
        }

        const auto &const_tree = *this->tree;

        const auto *min_val = const_tree.find_min();
        const auto *max_val = const_tree.find_max();

        ASSERT_NE(min_val, nullptr);
        ASSERT_NE(max_val, nullptr);
        EXPECT_TRUE(this->verify_value(*min_val, 0));
        EXPECT_TRUE(this->verify_value(*max_val, 49));
    }

    TYPED_TEST(BTreeTypedTest, ConstBounds)
    {
        for (int i = 0; i < 100; i += 10)
        {
            this->tree->insert(this->make_key(i), this->make_value(i));
        }

        const auto &const_tree = *this->tree;

        const auto *lb = const_tree.lower_bound(this->make_key(45));
        const auto *ub = const_tree.upper_bound(this->make_key(45));

        ASSERT_NE(lb, nullptr);
        ASSERT_NE(ub, nullptr);
        EXPECT_TRUE(this->verify_value(*lb, 50));
        EXPECT_TRUE(this->verify_value(*ub, 50));
    }

    // Size Verification
    TYPED_TEST(BTreeTypedTest, SizeAfterEveryOperation)
    {
        EXPECT_EQ(this->tree->size(), 0);

        this->tree->insert(this->make_key(1), this->make_value(1));
        EXPECT_EQ(this->tree->size(), 1);

        this->tree->insert(this->make_key(2), this->make_value(2));
        EXPECT_EQ(this->tree->size(), 2);

        this->tree->insert(this->make_key(1), this->make_value(99)); // Duplicate
        EXPECT_EQ(this->tree->size(), 2);                            // Should not increase

        this->tree->erase(this->make_key(1));
        EXPECT_EQ(this->tree->size(), 1);

        this->tree->erase(this->make_key(1)); // Delete non-existent
        EXPECT_EQ(this->tree->size(), 1);     // Should not change

        this->tree->erase(this->make_key(2));
        EXPECT_EQ(this->tree->size(), 0);
    }

    // Insertion Patterns
    TYPED_TEST(BTreeTypedTest, AlternatingPattern)
    {
        const int count = 50;

        // Alternating: 1, 100, 2, 99, 3, 98...
        for (int i = 0; i < count; ++i)
        {
            int low = i + 1;
            int high = 100 - i;

            this->tree->insert(this->make_key(low), this->make_value(low));
            if (low != high)
            {
                this->tree->insert(this->make_key(high), this->make_value(high));
            }
        }

        // Verify all keys
        for (int i = 1; i <= 100; ++i)
        {
            EXPECT_NE(this->tree->find(this->make_key(i)), nullptr)
                << "Key " << i << " not found";
        }
    }

    TYPED_TEST(BTreeTypedTest, ClusteredInsertions)
    {
        // Insert three clusters with gaps
        for (int i = 0; i < 10; ++i)
        {
            this->tree->insert(this->make_key(i), this->make_value(i));
        }

        for (int i = 90; i < 100; ++i)
        {
            this->tree->insert(this->make_key(i), this->make_value(i));
        }

        for (int i = 45; i < 55; ++i)
        {
            this->tree->insert(this->make_key(i), this->make_value(i));
        }

        EXPECT_EQ(this->tree->size(), 30);

        // Verify min/max
        auto *min_val = this->tree->find_min();
        auto *max_val = this->tree->find_max();
        ASSERT_NE(min_val, nullptr);
        ASSERT_NE(max_val, nullptr);
        EXPECT_TRUE(this->verify_value(*min_val, 0));
        EXPECT_TRUE(this->verify_value(*max_val, 99));
    }

    // Boundary Conditions
    TYPED_TEST(BTreeTypedTest, SingleKey)
    {
        this->tree->insert(this->make_key(1), this->make_value(1));

        EXPECT_EQ(this->tree->size(), 1);
        EXPECT_FALSE(this->tree->empty());

        auto *min_val = this->tree->find_min();
        auto *max_val = this->tree->find_max();
        ASSERT_NE(min_val, nullptr);
        ASSERT_NE(max_val, nullptr);
        EXPECT_TRUE(this->verify_value(*min_val, 1));
        EXPECT_TRUE(this->verify_value(*max_val, 1));

        EXPECT_TRUE(this->tree->erase(this->make_key(1)));
        EXPECT_TRUE(this->tree->empty());
    }

    TYPED_TEST(BTreeTypedTest, InsertExactlyMaxKeys)
    {
        // Insert exactly MAX_KEYS (should not trigger split)
        constexpr size_t MAX_KEYS = 15; // Order 16 - 1

        for (size_t i = 0; i < MAX_KEYS; ++i)
        {
            this->tree->insert(this->make_key(static_cast<int>(i)),
                               this->make_value(static_cast<int>(i)));
        }

        EXPECT_EQ(this->tree->size(), MAX_KEYS);

        // Verify all are findable
        for (size_t i = 0; i < MAX_KEYS; ++i)
        {
            EXPECT_NE(this->tree->find(this->make_key(static_cast<int>(i))), nullptr);
        }
    }

    TYPED_TEST(BTreeTypedTest, InsertOneMoreThanMaxKeys)
    {
        // Insert MAX_KEYS + 1 (should trigger split)
        constexpr size_t MAX_KEYS = 15;

        for (size_t i = 0; i <= MAX_KEYS; ++i)
        {
            this->tree->insert(this->make_key(static_cast<int>(i)),
                               this->make_value(static_cast<int>(i)));
        }

        EXPECT_EQ(this->tree->size(), MAX_KEYS + 1);

        // Verify all are findable
        for (size_t i = 0; i <= MAX_KEYS; ++i)
        {
            EXPECT_NE(this->tree->find(this->make_key(static_cast<int>(i))), nullptr);
        }
    }

} // namespace mcds::tests
