#include <gtest/gtest.h>
#include "mcds/b_tree.hpp"
#include "test_types/custom_types.hpp"

#include "test_suites/associative_container_typed_test.hpp"

namespace mcds::tests
{
    // Create a BTree-specific derived suite
    template <typename ContainerType>
    class BTreeTypedTest : public AssociativeContainerTypedTest<ContainerType>
    {
    };

    // Define all 9 concrete BTree type combinations
    using BTreeIntInt = BTree<int, int, 16, 1000>;
    using BTreeIntDouble = BTree<int, double, 16, 1000>;
    using BTreeDoubleInt = BTree<double, int, 16, 1000>;
    using BTreeIntComplex = BTree<int, ComplexType, 16, 1000>;
    using BTreeIntTracked = BTree<int, TrackedType, 16, 1000>;
    using BTreeComplexInt = BTree<ComplexType, int, 16, 1000>;
    using BTreeTrackedInt = BTree<TrackedType, int, 16, 1000>;
    using BTreeComplexComplex = BTree<ComplexType, ComplexType, 16, 1000>;
    using BTreeTrackedTracked = BTree<TrackedType, TrackedType, 16, 1000>;

    // Group all types together
    using BTreeTypes = ::testing::Types<
        BTreeIntInt,
        BTreeIntDouble,
        BTreeDoubleInt,
        BTreeIntComplex,
        BTreeIntTracked,
        BTreeComplexInt,
        BTreeTrackedInt,
        BTreeComplexComplex,
        BTreeTrackedTracked>;

    // Instantiate the test suite for all BTree types
    TYPED_TEST_SUITE(BTreeTypedTest, BTreeTypes);

    // Define test suite names so that they are reusable with other containers
#define CONTAINER_TEST_SUITE_NAME BTreeTypedTest
#include "test_suites/associative_container_test_suite.hpp"
#undef CONTAINER_TEST_SUITE_NAME

    // BTree specific tests
    TYPED_TEST(BTreeTypedTest, NodeMerge)
    {
        // Insert and then delete to trigger merges
        for (int i = 0; i < 50; ++i)
        {
            this->container->insert(this->make_key(i), this->make_value(i));
        }

        // Delete in a pattern that should trigger merges
        for (int i = 0; i < 40; ++i)
        {
            EXPECT_TRUE(this->container->erase(this->make_key(i)));
        }

        // Verify remaining keys
        for (int i = 40; i < 50; ++i)
        {
            EXPECT_NE(this->container->find(this->make_key(i)), nullptr);
        }
    }

    // Borrowing Tests
    TYPED_TEST(BTreeTypedTest, BorrowFromLeftSibling)
    {
        // Create a situation where borrowing from left is needed
        // Insert enough to create multiple nodes
        for (int i = 0; i < 50; ++i)
        {
            this->container->insert(this->make_key(i), this->make_value(i));
        }

        // Delete to create a node with MIN_KEYS, with left sibling having extras
        // This pattern should trigger borrow from left
        for (int i = 15; i < 25; ++i)
        {
            this->container->erase(this->make_key(i));
        }

        // Verify structure is still valid
        EXPECT_EQ(this->container->size(), 40);

        // Verify remaining keys are accessible
        for (int i = 0; i < 50; ++i)
        {
            if (i < 15 || i >= 25)
            {
                EXPECT_NE(this->container->find(this->make_key(i)), nullptr)
                    << "Key " << i << " should exist";
            }
            else
            {
                EXPECT_EQ(this->container->find(this->make_key(i)), nullptr)
                    << "Key " << i << " should not exist";
            }
        }
    }

    TYPED_TEST(BTreeTypedTest, BorrowFromRightSibling)
    {
        // Create situation where borrowing from right is needed
        for (int i = 0; i < 50; ++i)
        {
            this->container->insert(this->make_key(i), this->make_value(i));
        }

        // Delete from left side to trigger borrow from right
        for (int i = 0; i < 10; ++i)
        {
            this->container->erase(this->make_key(i));
        }

        EXPECT_EQ(this->container->size(), 40);

        for (int i = 10; i < 50; ++i)
        {
            EXPECT_NE(this->container->find(this->make_key(i)), nullptr);
        }
    }

    // Internal Node Deletion Paths
    TYPED_TEST(BTreeTypedTest, DeleteFromInternalUsePredecessor)
    {
        // Build tree tall enough to have internal nodes
        for (int i = 0; i < 100; ++i)
        {
            this->container->insert(this->make_key(i), this->make_value(i));
        }

        // Delete a key that should be in an internal node
        // After many insertions, middle keys often end up in internal nodes
        EXPECT_TRUE(this->container->erase(this->make_key(50)));

        // Verify it's gone and others remain
        EXPECT_EQ(this->container->find(this->make_key(50)), nullptr);
        EXPECT_NE(this->container->find(this->make_key(49)), nullptr);
        EXPECT_NE(this->container->find(this->make_key(51)), nullptr);
    }

    TYPED_TEST(BTreeTypedTest, DeleteFromInternalUseSuccessor)
    {
        // Similar to above but structured to favor successor path
        for (int i = 100; i >= 0; --i)
        {
            this->container->insert(this->make_key(i), this->make_value(i));
        }

        EXPECT_TRUE(this->container->erase(this->make_key(50)));
        EXPECT_EQ(this->container->find(this->make_key(50)), nullptr);
    }

    TYPED_TEST(BTreeTypedTest, DeleteFromInternalCausingMerge)
    {
        // Insert enough for internal nodes
        for (int i = 0; i < 100; ++i)
        {
            this->container->insert(this->make_key(i), this->make_value(i));
        }

        // Delete many keys to reduce tree and force merges
        for (int i = 0; i < 80; ++i)
        {
            this->container->erase(this->make_key(i));
        }

        EXPECT_EQ(this->container->size(), 20);

        // Verify remaining keys
        for (int i = 80; i < 100; ++i)
        {
            EXPECT_NE(this->container->find(this->make_key(i)), nullptr);
        }
    }

    // Root Special Cases
    TYPED_TEST(BTreeTypedTest, RootReplacementAfterDeletion)
    {
        // Insert enough to create child nodes
        for (int i = 0; i < 30; ++i)
        {
            this->container->insert(this->make_key(i), this->make_value(i));
        }

        size_t size_before = this->container->size();

        // Delete all but one key from root level, should cause root replacement
        for (int i = 0; i < 29; ++i)
        {
            this->container->erase(this->make_key(i));
        }

        EXPECT_EQ(this->container->size(), 1);
        EXPECT_NE(this->container->find(this->make_key(29)), nullptr);

        // Verify can still insert after root replacement
        this->container->insert(this->make_key(100), this->make_value(100));
        EXPECT_EQ(this->container->size(), 2);
    }

    TYPED_TEST(BTreeTypedTest, MultipleRootReplacements)
    {
        // Build up
        for (int i = 0; i < 50; ++i)
        {
            this->container->insert(this->make_key(i), this->make_value(i));
        }

        // Tear down to almost empty, causing multiple root changes
        for (int i = 0; i < 48; ++i)
        {
            this->container->erase(this->make_key(i));
        }

        EXPECT_EQ(this->container->size(), 2);

        // Build back up
        for (int i = 0; i < 50; ++i)
        {
            this->container->insert(this->make_key(i + 100), this->make_value(i + 100));
        }

        EXPECT_EQ(this->container->size(), 52);
    }

    TYPED_TEST(BTreeTypedTest, InsertExactlyMaxKeys)
    {
        // Insert exactly MAX_KEYS (should not trigger split)
        constexpr size_t MAX_KEYS = 15; // Order 16 - 1

        for (size_t i = 0; i < MAX_KEYS; ++i)
        {
            this->container->insert(this->make_key(static_cast<int>(i)),
                                    this->make_value(static_cast<int>(i)));
        }

        EXPECT_EQ(this->container->size(), MAX_KEYS);

        // Verify all are findable
        for (size_t i = 0; i < MAX_KEYS; ++i)
        {
            EXPECT_NE(this->container->find(this->make_key(static_cast<int>(i))), nullptr);
        }
    }

    TYPED_TEST(BTreeTypedTest, InsertOneMoreThanMaxKeys)
    {
        // Insert MAX_KEYS + 1 (should trigger split)
        constexpr size_t MAX_KEYS = 15;

        for (size_t i = 0; i <= MAX_KEYS; ++i)
        {
            this->container->insert(this->make_key(static_cast<int>(i)),
                                    this->make_value(static_cast<int>(i)));
        }

        EXPECT_EQ(this->container->size(), MAX_KEYS + 1);

        // Verify all are findable
        for (size_t i = 0; i <= MAX_KEYS; ++i)
        {
            EXPECT_NE(this->container->find(this->make_key(static_cast<int>(i))), nullptr);
        }
    }

} // namespace mcds::tests::btree
