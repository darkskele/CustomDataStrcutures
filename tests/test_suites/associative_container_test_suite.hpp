#ifndef CONTAINER_TEST_SUITE_NAME
#error "Must define CONTAINER_TEST_SUITE_NAME before including this file"
#endif

// Basic Operations Tests
TYPED_TEST(CONTAINER_TEST_SUITE_NAME, EmptyTree)
{
    EXPECT_TRUE(this->container->empty());
    EXPECT_EQ(this->container->size(), 0);
    EXPECT_EQ(this->container->find_min().second, nullptr);
    EXPECT_EQ(this->container->find_max().second, nullptr);
}

TYPED_TEST(CONTAINER_TEST_SUITE_NAME, SingleInsertAndFind)
{
    auto key = this->make_key(42);
    auto value = this->make_value(42);

    EXPECT_TRUE(this->container->insert(key, value));
    EXPECT_FALSE(this->container->empty());
    EXPECT_EQ(this->container->size(), 1);

    auto *found = this->container->find(key);
    ASSERT_NE(found, nullptr);
    EXPECT_TRUE(this->verify_value(*found, 42));
}

TYPED_TEST(CONTAINER_TEST_SUITE_NAME, MultipleInserts)
{
    const int count = 100;

    for (int i = 0; i < count; ++i)
    {
        EXPECT_TRUE(this->container->insert(this->make_key(i), this->make_value(i)));
    }

    EXPECT_EQ(this->container->size(), count);

    for (int i = 0; i < count; ++i)
    {
        auto *found = this->container->find(this->make_key(i));
        ASSERT_NE(found, nullptr) << "Failed to find key: " << i;
        EXPECT_TRUE(this->verify_value(*found, i));
    }
}

TYPED_TEST(CONTAINER_TEST_SUITE_NAME, InsertDuplicateUpdates)
{
    auto key = this->make_key(42);
    auto value1 = this->make_value(42);
    auto value2 = this->make_value(99);

    EXPECT_TRUE(this->container->insert(key, value1));
    EXPECT_EQ(this->container->size(), 1);

    EXPECT_TRUE(this->container->insert(key, value2));
    EXPECT_EQ(this->container->size(), 1); // Size shouldn't change

    auto *found = this->container->find(key);
    ASSERT_NE(found, nullptr);
    EXPECT_TRUE(this->verify_value(*found, 99)); // Should have updated value
}

TYPED_TEST(CONTAINER_TEST_SUITE_NAME, InsertReverseOrder)
{
    const int count = 100;

    for (int i = count - 1; i >= 0; --i)
    {
        EXPECT_TRUE(this->container->insert(this->make_key(i), this->make_value(i)));
    }

    EXPECT_EQ(this->container->size(), count);

    for (int i = 0; i < count; ++i)
    {
        auto *found = this->container->find(this->make_key(i));
        ASSERT_NE(found, nullptr);
        EXPECT_TRUE(this->verify_value(*found, i));
    }
}

TYPED_TEST(CONTAINER_TEST_SUITE_NAME, InsertRandomOrder)
{
    const int count = 100;
    std::vector<int> values(count);
    std::iota(values.begin(), values.end(), 0);

    std::random_device rd;
    std::mt19937 gen(42); // Fixed seed for reproducibility
    std::shuffle(values.begin(), values.end(), gen);

    for (int val : values)
    {
        EXPECT_TRUE(this->container->insert(this->make_key(val), this->make_value(val)));
    }

    EXPECT_EQ(this->container->size(), count);

    for (int i = 0; i < count; ++i)
    {
        auto *found = this->container->find(this->make_key(i));
        ASSERT_NE(found, nullptr);
        EXPECT_TRUE(this->verify_value(*found, i));
    }
}

TYPED_TEST(CONTAINER_TEST_SUITE_NAME, FindNonExistent)
{
    this->container->insert(this->make_key(10), this->make_value(10));
    this->container->insert(this->make_key(20), this->make_value(20));
    this->container->insert(this->make_key(30), this->make_value(30));

    EXPECT_EQ(this->container->find(this->make_key(5)), nullptr);
    EXPECT_EQ(this->container->find(this->make_key(15)), nullptr);
    EXPECT_EQ(this->container->find(this->make_key(25)), nullptr);
    EXPECT_EQ(this->container->find(this->make_key(35)), nullptr);
}

// Deletion Tests
TYPED_TEST(CONTAINER_TEST_SUITE_NAME, DeleteSingle)
{
    auto key = this->make_key(42);
    this->container->insert(key, this->make_value(42));

    EXPECT_TRUE(this->container->erase(key));
    EXPECT_TRUE(this->container->empty());
    EXPECT_EQ(this->container->size(), 0);
    EXPECT_EQ(this->container->find(key), nullptr);
}

TYPED_TEST(CONTAINER_TEST_SUITE_NAME, DeleteNonExistent)
{
    this->container->insert(this->make_key(10), this->make_value(10));

    EXPECT_FALSE(this->container->erase(this->make_key(20)));
    EXPECT_EQ(this->container->size(), 1);
}

TYPED_TEST(CONTAINER_TEST_SUITE_NAME, DeleteFromLeaf)
{
    for (int i = 0; i < 50; ++i)
    {
        this->container->insert(this->make_key(i), this->make_value(i));
    }

    EXPECT_TRUE(this->container->erase(this->make_key(25)));
    EXPECT_EQ(this->container->size(), 49);
    EXPECT_EQ(this->container->find(this->make_key(25)), nullptr);

    // Verify other keys still exist
    for (int i = 0; i < 50; ++i)
    {
        if (i != 25)
        {
            EXPECT_NE(this->container->find(this->make_key(i)), nullptr);
        }
    }
}

TYPED_TEST(CONTAINER_TEST_SUITE_NAME, DeleteMultiple)
{
    const int count = 100;

    for (int i = 0; i < count; ++i)
    {
        EXPECT_TRUE(this->container->insert(this->make_key(i), this->make_value(i))) << "Failed to insert on key " << i << std::endl;
    }

    // Delete every other element
    for (int i = 0; i < count; i += 2)
    {
        EXPECT_TRUE(this->container->erase(this->make_key(i))) << "Failed erase on key " << i << std::endl;
    }

    EXPECT_EQ(this->container->size(), count / 2);

    // Verify deleted keys are gone
    for (int i = 0; i < count; i += 2)
    {
        EXPECT_EQ(this->container->find(this->make_key(i)), nullptr);
    }

    // Verify remaining keys exist
    for (int i = 1; i < count; i += 2)
    {
        EXPECT_NE(this->container->find(this->make_key(i)), nullptr);
    }
}

TYPED_TEST(CONTAINER_TEST_SUITE_NAME, DeleteAll)
{
    const int count = 100;

    for (int i = 0; i < count; ++i)
    {
        EXPECT_TRUE(this->container->insert(this->make_key(i), this->make_value(i)));
    }

    for (int i = 0; i < count; ++i)
    {
        EXPECT_TRUE(this->container->erase(this->make_key(i)));
    }

    EXPECT_TRUE(this->container->empty());
    EXPECT_EQ(this->container->size(), 0);
}

TYPED_TEST(CONTAINER_TEST_SUITE_NAME, DeleteAndReinsert)
{
    auto key = this->make_key(42);
    auto value1 = this->make_value(42);
    auto value2 = this->make_value(99);

    this->container->insert(key, value1);
    EXPECT_TRUE(this->container->erase(key));
    EXPECT_EQ(this->container->find(key), nullptr);

    this->container->insert(key, value2);
    auto *found = this->container->find(key);
    ASSERT_NE(found, nullptr);
    EXPECT_TRUE(this->verify_value(*found, 99));
}

// Min/Max Tests
TYPED_TEST(CONTAINER_TEST_SUITE_NAME, FindMinMax)
{
    this->container->insert(this->make_key(50), this->make_value(50));
    this->container->insert(this->make_key(25), this->make_value(25));
    this->container->insert(this->make_key(75), this->make_value(75));
    this->container->insert(this->make_key(10), this->make_value(10));
    this->container->insert(this->make_key(90), this->make_value(90));

    auto min_val = this->container->find_min();
    auto max_val = this->container->find_max();

    ASSERT_NE(min_val.second, nullptr);
    ASSERT_NE(max_val.second, nullptr);
    EXPECT_TRUE(this->verify_value(*(min_val.second), 10));
    EXPECT_TRUE(this->verify_value(*(max_val.second), 90));
}

TYPED_TEST(CONTAINER_TEST_SUITE_NAME, FindMinMaxAfterDeletions)
{
    for (int i = 0; i < 100; ++i)
    {
        this->container->insert(this->make_key(i), this->make_value(i));
    }

    this->container->erase(this->make_key(0));
    this->container->erase(this->make_key(99));

    auto min_val = this->container->find_min();
    auto max_val = this->container->find_max();

    ASSERT_NE(min_val.second, nullptr);
    ASSERT_NE(max_val.second, nullptr);
    EXPECT_TRUE(this->verify_value(*(min_val.second), 1));
    EXPECT_TRUE(this->verify_value(*(max_val.second), 98));
}

// Lower/Upper Bound Tests
TYPED_TEST(CONTAINER_TEST_SUITE_NAME, LowerBoundExactMatch)
{
    for (int i = 0; i < 100; i += 10)
    {
        this->container->insert(this->make_key(i), this->make_value(i));
    }

    auto *result = this->container->lower_bound(this->make_key(50));
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(this->verify_key(*result, 50));
}

TYPED_TEST(CONTAINER_TEST_SUITE_NAME, LowerBoundBetweenKeys)
{
    for (int i = 0; i < 100; i += 10)
    {
        this->container->insert(this->make_key(i), this->make_value(i));
    }

    auto *result = this->container->lower_bound(this->make_key(55));
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(this->verify_key(*result, 60));
}

TYPED_TEST(CONTAINER_TEST_SUITE_NAME, LowerBoundNotFound)
{
    for (int i = 0; i < 100; i += 10)
    {
        this->container->insert(this->make_key(i), this->make_value(i));
    }

    auto *result = this->container->lower_bound(this->make_key(100));
    EXPECT_EQ(result, nullptr);
}

TYPED_TEST(CONTAINER_TEST_SUITE_NAME, UpperBoundExactMatch)
{
    for (int i = 0; i < 100; i += 10)
    {
        this->container->insert(this->make_key(i), this->make_value(i));
    }

    auto *result = this->container->upper_bound(this->make_key(50));
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(this->verify_key(*result, 60));
}

TYPED_TEST(CONTAINER_TEST_SUITE_NAME, UpperBoundBetweenKeys)
{
    for (int i = 0; i < 100; i += 10)
    {
        this->container->insert(this->make_key(i), this->make_value(i));
    }

    auto *result = this->container->upper_bound(this->make_key(55));
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(this->verify_key(*result, 60));
}

TYPED_TEST(CONTAINER_TEST_SUITE_NAME, UpperBoundNotFound)
{
    for (int i = 0; i < 100; i += 10)
    {
        this->container->insert(this->make_key(i), this->make_value(i));
    }

    auto *result = this->container->upper_bound(this->make_key(90));
    EXPECT_EQ(result, nullptr);
}

// Operator[] Tests
TYPED_TEST(CONTAINER_TEST_SUITE_NAME, SubscriptOperatorInsert)
{
    auto key = this->make_key(42);
    auto &value = (*this->container)[key];

    // Value should be default-constructed
    EXPECT_EQ(this->container->size(), 1);
}

TYPED_TEST(CONTAINER_TEST_SUITE_NAME, SubscriptOperatorAccess)
{
    auto key = this->make_key(42);
    this->container->insert(key, this->make_value(42));

    auto &value = (*this->container)[key];
    EXPECT_TRUE(this->verify_value(value, 42));
}

// Stress Tests
TYPED_TEST(CONTAINER_TEST_SUITE_NAME, StressTestLargeInsertion)
{
    const int count = 1000;

    for (int i = 0; i < count; ++i)
    {
        if(!this->container->insert(this->make_key(i), this->make_value(i)))
        {
            EXPECT_TRUE(false) << "Failed to insert !";
        }
    }

    EXPECT_EQ(this->container->size(), count);

    // Spot check
    for (int i = 0; i < count; i += 10)
    {
        EXPECT_NE(this->container->find(this->make_key(i)), nullptr);
    }
}

TYPED_TEST(CONTAINER_TEST_SUITE_NAME, StressTestMixedOperations)
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
            EXPECT_TRUE(this->container->insert(this->make_key(key_int), this->make_value(key_int))) << "Operation =" + std::to_string(op);
            inserted_keys.insert(key_int);
        }
        else if (operation == 1) // Delete
        {
            bool should_exist = inserted_keys.count(key_int) > 0;
            EXPECT_EQ(this->container->erase(this->make_key(key_int)), should_exist) << "Operation =" + std::to_string(op);
            inserted_keys.erase(key_int);
        }
        else // Find
        {
            auto *result = this->container->find(this->make_key(key_int));
            bool should_exist = inserted_keys.count(key_int) > 0;
            EXPECT_EQ(result != nullptr, should_exist) << "Operation =" + std::to_string(op);
        }
    }

    // Verify final state
    EXPECT_EQ(this->container->size(), inserted_keys.size());
}

// Edge Cases
TYPED_TEST(CONTAINER_TEST_SUITE_NAME, RootSplit)
{
    // Insert enough to cause root split
    const int keys_to_insert = 30;

    for (int i = 0; i < keys_to_insert; ++i)
    {
        this->container->insert(this->make_key(i), this->make_value(i));
    }

    // Verify all keys are still accessible
    for (int i = 0; i < keys_to_insert; ++i)
    {
        EXPECT_NE(this->container->find(this->make_key(i)), nullptr);
    }
}

TYPED_TEST(CONTAINER_TEST_SUITE_NAME, Redistribution)
{
    // Pattern designed to trigger redistribution
    for (int i = 0; i < 100; ++i)
    {
        this->container->insert(this->make_key(i), this->make_value(i));
    }

    // Delete keys to trigger redistribution
    for (int i = 30; i < 70; ++i)
    {
        this->container->erase(this->make_key(i));
    }

    // Verify structure integrity
    for (int i = 0; i < 30; ++i)
    {
        EXPECT_NE(this->container->find(this->make_key(i)), nullptr);
    }
    for (int i = 70; i < 100; ++i)
    {
        EXPECT_NE(this->container->find(this->make_key(i)), nullptr);
    }
}

// Lower/Upper Bound Edge Cases
TYPED_TEST(CONTAINER_TEST_SUITE_NAME, LowerBoundOnEmptyTree)
{
    EXPECT_EQ(this->container->lower_bound(this->make_key(10)), nullptr);
}

TYPED_TEST(CONTAINER_TEST_SUITE_NAME, LowerBoundSmallerThanAll)
{
    for (int i = 10; i < 100; i += 10)
    {
        this->container->insert(this->make_key(i), this->make_value(i));
    }

    auto *result = this->container->lower_bound(this->make_key(5));
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(this->verify_key(*result, 10));
}

TYPED_TEST(CONTAINER_TEST_SUITE_NAME, UpperBoundLargerThanAll)
{
    for (int i = 0; i < 100; i += 10)
    {
        this->container->insert(this->make_key(i), this->make_value(i));
    }

    auto *result = this->container->upper_bound(this->make_key(100));
    EXPECT_EQ(result, nullptr);
}

TYPED_TEST(CONTAINER_TEST_SUITE_NAME, BoundsOnSingleElement)
{
    this->container->insert(this->make_key(50), this->make_value(50));

    // Lower bound on exact match
    auto *lb_exact = this->container->lower_bound(this->make_key(50));
    ASSERT_NE(lb_exact, nullptr);
    EXPECT_TRUE(this->verify_key(*lb_exact, 50));

    // Lower bound smaller
    auto *lb_smaller = this->container->lower_bound(this->make_key(40));
    ASSERT_NE(lb_smaller, nullptr);
    EXPECT_TRUE(this->verify_key(*lb_smaller, 50));

    // Lower bound larger
    auto *lb_larger = this->container->lower_bound(this->make_key(60));
    EXPECT_EQ(lb_larger, nullptr);

    // Upper bound
    auto *ub = this->container->upper_bound(this->make_key(50));
    EXPECT_EQ(ub, nullptr);

    auto *ub_smaller = this->container->upper_bound(this->make_key(40));
    ASSERT_NE(ub_smaller, nullptr);
    EXPECT_TRUE(this->verify_key(*ub_smaller, 50));
}

TYPED_TEST(CONTAINER_TEST_SUITE_NAME, BoundsWithGaps)
{
    // Insert with gaps
    this->container->insert(this->make_key(10), this->make_value(10));
    this->container->insert(this->make_key(20), this->make_value(20));
    this->container->insert(this->make_key(40), this->make_value(40));
    this->container->insert(this->make_key(50), this->make_value(50));

    // Test in gaps
    auto *lb_15 = this->container->lower_bound(this->make_key(15));
    ASSERT_NE(lb_15, nullptr);
    EXPECT_TRUE(this->verify_key(*lb_15, 20));

    auto *lb_35 = this->container->lower_bound(this->make_key(35));
    ASSERT_NE(lb_35, nullptr);
    EXPECT_TRUE(this->verify_key(*lb_35, 40));

    auto *ub_35 = this->container->upper_bound(this->make_key(35));
    ASSERT_NE(ub_35, nullptr);
    EXPECT_TRUE(this->verify_key(*ub_35, 40));
}

// Value Modification
TYPED_TEST(CONTAINER_TEST_SUITE_NAME, ModifyValueThroughPointer)
{
    auto key = this->make_key(42);
    this->container->insert(key, this->make_value(42));

    auto *val = this->container->find(key);
    ASSERT_NE(val, nullptr);

    // Modify through pointer
    *val = this->make_value(99);

    // Verify modification persisted
    auto *val2 = this->container->find(key);
    ASSERT_NE(val2, nullptr);
    EXPECT_TRUE(this->verify_value(*val2, 99));
}

TYPED_TEST(CONTAINER_TEST_SUITE_NAME, ModifyValueThroughSubscript)
{
    auto key = this->make_key(42);
    (*this->container)[key] = this->make_value(42);

    // Modify
    (*this->container)[key] = this->make_value(99);

    // Verify
    EXPECT_TRUE(this->verify_value((*this->container)[key], 99));
}

// Const Correctness
TYPED_TEST(CONTAINER_TEST_SUITE_NAME, ConstMinMax)
{
    for (int i = 0; i < 50; ++i)
    {
        this->container->insert(this->make_key(i), this->make_value(i));
    }

    const auto min_val = this->container->find_min();
    const auto max_val = this->container->find_max();

    ASSERT_NE(min_val.second, nullptr);
    ASSERT_NE(max_val.second, nullptr);
    EXPECT_TRUE(this->verify_value(*(min_val.second), 0));
    EXPECT_TRUE(this->verify_value(*(max_val.second), 49));
}

TYPED_TEST(CONTAINER_TEST_SUITE_NAME, ConstBounds)
{
    for (int i = 0; i < 100; i += 10)
    {
        this->container->insert(this->make_key(i), this->make_value(i));
    }

    const auto *lb = this->container->lower_bound(this->make_key(45));
    const auto *ub = this->container->upper_bound(this->make_key(45));

    ASSERT_NE(lb, nullptr);
    ASSERT_NE(ub, nullptr);
    EXPECT_TRUE(this->verify_key(*lb, 50));
    EXPECT_TRUE(this->verify_key(*ub, 50));
}

// Size Verification
TYPED_TEST(CONTAINER_TEST_SUITE_NAME, SizeAfterEveryOperation)
{
    EXPECT_EQ(this->container->size(), 0);

    this->container->insert(this->make_key(1), this->make_value(1));
    EXPECT_EQ(this->container->size(), 1);

    this->container->insert(this->make_key(2), this->make_value(2));
    EXPECT_EQ(this->container->size(), 2);

    this->container->insert(this->make_key(1), this->make_value(99)); // Duplicate
    EXPECT_EQ(this->container->size(), 2);                            // Should not increase

    this->container->erase(this->make_key(1));
    EXPECT_EQ(this->container->size(), 1);

    this->container->erase(this->make_key(1)); // Delete non-existent
    EXPECT_EQ(this->container->size(), 1);     // Should not change

    this->container->erase(this->make_key(2));
    EXPECT_EQ(this->container->size(), 0);
}

// Insertion Patterns
TYPED_TEST(CONTAINER_TEST_SUITE_NAME, AlternatingPattern)
{
    const int count = 50;

    // Alternating: 1, 100, 2, 99, 3, 98...
    for (int i = 0; i < count; ++i)
    {
        int low = i + 1;
        int high = 100 - i;

        this->container->insert(this->make_key(low), this->make_value(low));
        if (low != high)
        {
            this->container->insert(this->make_key(high), this->make_value(high));
        }
    }

    // Verify all keys
    for (int i = 1; i <= 100; ++i)
    {
        EXPECT_NE(this->container->find(this->make_key(i)), nullptr)
            << "Key " << i << " not found";
    }
}

TYPED_TEST(CONTAINER_TEST_SUITE_NAME, ClusteredInsertions)
{
    // Insert three clusters with gaps
    for (int i = 0; i < 10; ++i)
    {
        this->container->insert(this->make_key(i), this->make_value(i));
    }

    for (int i = 90; i < 100; ++i)
    {
        this->container->insert(this->make_key(i), this->make_value(i));
    }

    for (int i = 45; i < 55; ++i)
    {
        this->container->insert(this->make_key(i), this->make_value(i));
    }

    EXPECT_EQ(this->container->size(), 30);

    // Verify min/max
    auto min_val = this->container->find_min();
    auto max_val = this->container->find_max();
    ASSERT_NE(min_val.second, nullptr);
    ASSERT_NE(max_val.second, nullptr);
    EXPECT_TRUE(this->verify_value(*(min_val.second), 0));
    EXPECT_TRUE(this->verify_value(*(max_val.second), 99));
}

// Boundary Conditions
TYPED_TEST(CONTAINER_TEST_SUITE_NAME, SingleKey)
{
    this->container->insert(this->make_key(1), this->make_value(1));

    EXPECT_EQ(this->container->size(), 1);
    EXPECT_FALSE(this->container->empty());

    auto min_val = this->container->find_min();
    auto max_val = this->container->find_max();
    ASSERT_NE(min_val.second, nullptr);
    ASSERT_NE(max_val.second, nullptr);
    EXPECT_TRUE(this->verify_value(*(min_val.second), 1));
    EXPECT_TRUE(this->verify_value(*(max_val.second), 1));

    EXPECT_TRUE(this->container->erase(this->make_key(1)));
    EXPECT_TRUE(this->container->empty());
}
