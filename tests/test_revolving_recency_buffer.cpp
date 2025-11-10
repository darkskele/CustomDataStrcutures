#include <gtest/gtest.h>
#include "mcds/revolving_ring_buffer.hpp"
#include <vector>
#include <numeric>

using namespace btc_stream::streamer::buffers;

template <typename T, size_t N>
using buffer_t = revolving_recency_buffer<T, N>;

// EMPTY CONSTRUCTION
TEST(RevolvingRecencyBufferTest, EmptyConstructedState)
{
    buffer_t<int, 3> buf;
    EXPECT_EQ(buf.size(), 0);
    EXPECT_TRUE(buf.empty());
    EXPECT_FALSE(buf.full());
    EXPECT_EQ(buf.capicity(), 3);
}

TEST(RevolvingRecencyBufferTest, EmptyAtThrows)
{
    buffer_t<int, 3> buf;
    EXPECT_THROW(buf.at(0), std::out_of_range);
}

TEST(RevolvingRecencyBufferTest, EmptyOperatorDeath)
{
    buffer_t<int, 3> buf;
    EXPECT_DEATH(buf[0], ".*revolving_recency_buffer out of range.*");
}

TEST(RevolvingRecencyBufferTest, EmptyFrontBackDeath)
{
    buffer_t<int, 3> buf;
    EXPECT_DEATH(buf.front(), ".*Front called on empty buffer.*");
    EXPECT_DEATH(buf.back(), ".*Back called on empty buffer.*");
}

TEST(RevolvingRecencyBufferTest, EmptyClearNoCrash)
{
    buffer_t<int, 3> buf;
    buf.clear();
    EXPECT_TRUE(buf.empty());
    EXPECT_EQ(buf.size(), 0);
}

TEST(RevolvingRecencyBufferTest, EmptyIteration)
{
    revolving_recency_buffer<int, 3> buf;
    int count = 0;
    for (auto &v : buf)
    {
        (void)v;
        ++count;
    }
    EXPECT_EQ(count, 0);
}

// Basic fill
TEST(RevolvingRecencyBufferTest, SingleElementState)
{
    buffer_t<int, 3> buf;
    buf.push(42);
    EXPECT_EQ(buf.size(), 1);
    EXPECT_EQ(buf.front(), 42);
    EXPECT_EQ(buf.back(), 42);
    EXPECT_EQ(buf[0], 42);
}

TEST(RevolvingRecencyBufferTest, TwoElementState)
{
    buffer_t<int, 3> buf;
    buf.push(1);
    buf.push(2);
    EXPECT_EQ(buf.size(), 2);
    EXPECT_EQ(buf.front(), 2);
    EXPECT_EQ(buf.back(), 1);
    EXPECT_EQ(buf[0], 2);
    EXPECT_EQ(buf[1], 1);
}

TEST(RevolvingRecencyBufferTest, PartialFillIterationOrder)
{
    buffer_t<int, 3> buf;
    buf.push(10);
    buf.push(20);
    std::vector<int> values;
    for (auto &v : buf)
        values.push_back(v);
    EXPECT_EQ(values, (std::vector<int>{20, 10}));
}

TEST(RevolvingRecencyBufferTest, PartialFillAtAndThrows)
{
    buffer_t<int, 3> buf;
    buf.push(99);
    buf.push(100);
    EXPECT_EQ(buf.at(0), 100);
    EXPECT_EQ(buf.at(1), 99);
    EXPECT_THROW(buf.at(2), std::out_of_range);
}

TEST(RevolvingRecencyBufferTest, PartialFillClearResets)
{
    buffer_t<int, 3> buf;
    buf.push(5);
    buf.push(6);
    buf.clear();
    EXPECT_TRUE(buf.empty());
    EXPECT_EQ(buf.size(), 0);
    buf.push(7);
    EXPECT_EQ(buf.front(), 7);
    EXPECT_EQ(buf.back(), 7);
}

// Full Buffer
TEST(RevolvingRecencyBufferTest, FillToCapacityState)
{
    buffer_t<int, 3> buf;
    buf.push(1);
    buf.push(2);
    buf.push(3);

    EXPECT_EQ(buf.size(), 3);
    EXPECT_TRUE(buf.full());
    EXPECT_FALSE(buf.empty());

    EXPECT_EQ(buf.front(), 3); // newest
    EXPECT_EQ(buf.back(), 1);  // oldest
}

TEST(RevolvingRecencyBufferTest, FillToCapacityIndexing)
{
    buffer_t<int, 3> buf;
    buf.push(1);
    buf.push(2);
    buf.push(3);
    EXPECT_EQ(buf[0], 3);
    EXPECT_EQ(buf[1], 2);
    EXPECT_EQ(buf[2], 1);
    EXPECT_THROW(buf.at(3), std::out_of_range);
    EXPECT_DEATH(buf[3], ".*revolving_recency_buffer out of range.*");
}

TEST(RevolvingRecencyBufferTest, FullBufferIteration)
{
    buffer_t<int, 3> buf;
    buf.push(1);
    buf.push(2);
    buf.push(3);

    std::vector<int> values;
    for (auto &v : buf)
        values.push_back(v);
    EXPECT_EQ(values, (std::vector<int>{3, 2, 1}));
}

TEST(RevolvingRecencyBufferTest, FullBufferClearResets)
{
    buffer_t<int, 3> buf;
    buf.push(1);
    buf.push(2);
    buf.push(3);
    buf.clear();
    EXPECT_TRUE(buf.empty());
    EXPECT_EQ(buf.size(), 0);
    buf.push(9);
    EXPECT_EQ(buf.front(), 9);
    EXPECT_EQ(buf.back(), 9);
}

// Overfill, writes over oldest
TEST(RevolvingRecencyBufferTest, OverfillOneBeyondCapacity)
{
    buffer_t<int, 3> buf;
    buf.push(1);
    buf.push(2);
    buf.push(3);
    buf.push(4); // overwrites 1

    EXPECT_EQ(buf.size(), 3);
    EXPECT_TRUE(buf.full());

    // Now buffer holds [4,3,2] (front=newest=4, back=oldest=2)
    EXPECT_EQ(buf.front(), 4);
    EXPECT_EQ(buf.back(), 2);

    EXPECT_EQ(buf[0], 4);
    EXPECT_EQ(buf[1], 3);
    EXPECT_EQ(buf[2], 2);

    std::vector<int> values;
    for (auto &v : buf)
        values.push_back(v);
    EXPECT_EQ(values, (std::vector<int>{4, 3, 2}));
}

TEST(RevolvingRecencyBufferTest, OverfillTwiceBeyondCapacity)
{
    buffer_t<int, 3> buf;
    buf.push(10);
    buf.push(20);
    buf.push(30);
    buf.push(40); // evict 10
    buf.push(50); // evict 20

    EXPECT_EQ(buf.size(), 3);
    EXPECT_TRUE(buf.full());

    // Buffer should now be [50,40,30]
    EXPECT_EQ(buf.front(), 50);
    EXPECT_EQ(buf.back(), 30);

    EXPECT_EQ(buf[0], 50);
    EXPECT_EQ(buf[1], 40);
    EXPECT_EQ(buf[2], 30);
}

TEST(RevolvingRecencyBufferTest, OverfillManyTimesStability)
{
    buffer_t<int, 3> buf;
    for (int i = 1; i <= 10; ++i)
        buf.push(i);

    EXPECT_EQ(buf.size(), 3);
    EXPECT_TRUE(buf.full());

    // Should only hold last 3 values [10,9,8]
    EXPECT_EQ(buf.front(), 10);
    EXPECT_EQ(buf.back(), 8);

    std::vector<int> values;
    for (auto &v : buf)
        values.push_back(v);
    EXPECT_EQ(values, (std::vector<int>{10, 9, 8}));
}

// Clear after overwrite
TEST(RevolvingRecencyBufferTest, ClearAfterOverwriteResets)
{
    buffer_t<int, 3> buf;
    buf.push(1);
    buf.push(2);
    buf.push(3);
    buf.push(4); // overwrite 1

    buf.clear();

    EXPECT_TRUE(buf.empty());
    EXPECT_EQ(buf.size(), 0);
    EXPECT_FALSE(buf.full());

    buf.push(99);
    EXPECT_EQ(buf.front(), 99);
    EXPECT_EQ(buf.back(), 99);
}

// Iterator robustness
TEST(RevolvingRecencyBufferTest, IteratorAfterOverwrites)
{
    buffer_t<int, 3> buf;
    buf.push(1);
    buf.push(2);
    buf.push(3);
    buf.push(4); // evicts 1

    std::vector<int> values;
    for (auto it = buf.begin(); it != buf.end(); ++it)
    {
        values.push_back(*it);
    }
    EXPECT_EQ(values, (std::vector<int>{4, 3, 2}));
}

TEST(RevolvingRecencyBufferTest, ConstIteratorComparison)
{
    buffer_t<int, 3> buf;
    buf.push(10);
    buf.push(20);

    auto it = buf.begin();
    auto cit = buf.cbegin();

    // Both should point to newest element
    EXPECT_EQ(*it, *cit);
    EXPECT_TRUE(it == cit);
    EXPECT_FALSE(it != cit);

    ++it;
    ++cit;
    EXPECT_EQ(*it, *cit);
    EXPECT_TRUE(it == cit);
}

TEST(RevolvingRecencyBufferTest, IterationAfterClear)
{
    buffer_t<int, 3> buf;
    buf.push(1);
    buf.push(2);
    buf.push(3);
    buf.clear();

    std::vector<int> values;
    for (auto &v : buf)
        values.push_back(v);

    EXPECT_TRUE(values.empty());
    EXPECT_EQ(buf.begin(), buf.end());
}

TEST(RevolvingRecencyBufferTest, EmptyBufferIterationEquality)
{
    buffer_t<int, 3> buf;
    auto it = buf.begin();
    auto end = buf.end();
    EXPECT_TRUE(it == end);
}

// Track construction and destructor
struct Tracked
{
    static inline int ctor_count = 0;
    static inline int dtor_count = 0;
    int value;

    Tracked(int v = 0) noexcept : value(v) { ++ctor_count; }
    Tracked(const Tracked &other) : value(other.value) { ++ctor_count; }
    Tracked(Tracked &&other) noexcept : value(other.value) { ++ctor_count; }
    ~Tracked() { ++dtor_count; }

    Tracked &operator=(const Tracked &) = default;
    Tracked &operator=(Tracked &&) noexcept = default;

    static void reset()
    {
        ctor_count = 0;
        dtor_count = 0;
    }
};

// Resource management
TEST(RevolvingRecencyBufferTest, ConstructionsAndDestructionsOnPush)
{
    Tracked::reset();
    {
        buffer_t<Tracked, 3> buf;
        buf.push(Tracked(1));
        buf.push(Tracked(2));
        buf.push(Tracked(3));

        EXPECT_EQ(buf.size(), 3);
        EXPECT_EQ(Tracked::ctor_count, 6); // 3 temporaries + 3 in-place
        EXPECT_EQ(Tracked::dtor_count, 3); // 3 temporaries destroyed
    }
    // buffer goes out of scope -> remaining 3 destroyed
    EXPECT_EQ(Tracked::dtor_count, 6);
}

TEST(RevolvingRecencyBufferTest, DestructorDestroysAll)
{
    Tracked::reset();
    {
        buffer_t<Tracked, 2> buf;
        buf.emplace(10);
        buf.emplace(20);
        EXPECT_EQ(buf.size(), 2);
        EXPECT_EQ(Tracked::ctor_count, 2);
        EXPECT_EQ(Tracked::dtor_count, 0);
    }
    EXPECT_EQ(Tracked::dtor_count, 2);
}

TEST(RevolvingRecencyBufferTest, OverwriteDestroysOldest)
{
    Tracked::reset();
    {
        buffer_t<Tracked, 2> buf;
        buf.emplace(1);
        buf.emplace(2);
        buf.emplace(3); // overwrites 1 -> destroy 1

        EXPECT_EQ(buf.size(), 2);
        EXPECT_EQ(buf.front().value, 3);
        EXPECT_EQ(buf.back().value, 2);

        EXPECT_EQ(Tracked::ctor_count, 3);
        EXPECT_EQ(Tracked::dtor_count, 1);
    }
    // Remaining 2 destroyed when buf goes out of scope
    EXPECT_EQ(Tracked::dtor_count, 3);
}

TEST(RevolvingRecencyBufferTest, ClearDestroysAll)
{
    Tracked::reset();
    buffer_t<Tracked, 3> buf;
    buf.emplace(1);
    buf.emplace(2);
    buf.emplace(3);

    EXPECT_EQ(buf.size(), 3);
    EXPECT_EQ(Tracked::dtor_count, 0);

    buf.clear();

    EXPECT_EQ(buf.size(), 0);
    EXPECT_EQ(Tracked::dtor_count, 3);
}

// Mixed push and emplace consistency
TEST(RevolvingRecencyBufferTest, PushConstLValue)
{
    buffer_t<int, 3> buf;
    int a = 42;
    buf.push(a); // copy

    EXPECT_EQ(buf.size(), 1);
    EXPECT_EQ(buf.front(), 42);
    EXPECT_EQ(buf.back(), 42);
}

TEST(RevolvingRecencyBufferTest, PushRValue)
{
    buffer_t<std::string, 3> buf;
    buf.push(std::string("hello")); // move

    EXPECT_EQ(buf.size(), 1);
    EXPECT_EQ(buf.front(), "hello");
    EXPECT_EQ(buf.back(), "hello");
}

TEST(RevolvingRecencyBufferTest, EmplaceDirectConstruction)
{
    buffer_t<std::pair<int, int>, 2> buf;
    buf.emplace(1, 10); // constructs in-place
    buf.emplace(2, 20);

    EXPECT_EQ(buf.size(), 2);
    EXPECT_EQ(buf.front(), std::make_pair(2, 20));
    EXPECT_EQ(buf.back(), std::make_pair(1, 10));
}

TEST(RevolvingRecencyBufferTest, OverwriteConsistentAcrossApis)
{
    buffer_t<int, 2> buf;
    int x = 1;
    buf.push(x);    // push(const&)
    buf.push(2);    // push(rvalue)
    buf.emplace(3); // emplace → overwrites 1

    EXPECT_EQ(buf.size(), 2);
    EXPECT_EQ(buf.front(), 3); // newest
    EXPECT_EQ(buf.back(), 2);  // oldest remaining
}

TEST(RevolvingRecencyBufferTest, KeepsOnlyLastNElements)
{
    constexpr size_t N = 5;
    buffer_t<int, N> buf;

    for (int i = 1; i <= 50; ++i)
    {
        buf.push(i);
    }

    EXPECT_EQ(buf.size(), N);
    // Expect newest = 50, oldest = 46
    EXPECT_EQ(buf.front(), 50);
    EXPECT_EQ(buf.back(), 46);

    std::vector<int> values;
    for (auto &v : buf)
        values.push_back(v);
    EXPECT_EQ(values, (std::vector<int>{50, 49, 48, 47, 46}));
}

TEST(RevolvingRecencyBufferTest, ConstAccessorsWork)
{
    buffer_t<int, 3> buf;
    buf.push(10);
    buf.push(20);
    buf.push(30);

    const auto &cbuf = buf;

    EXPECT_EQ(cbuf.size(), 3);
    EXPECT_EQ(cbuf.front(), 30);
    EXPECT_EQ(cbuf.back(), 10);

    std::vector<int> values;
    for (auto it = cbuf.cbegin(); it != cbuf.cend(); ++it)
    {
        values.push_back(*it);
    }
    EXPECT_EQ(values, (std::vector<int>{30, 20, 10}));
}

TEST(RevolvingRecencyBufferTest, CapacityOneBuffer)
{
    buffer_t<int, 1> buf;

    buf.push(100);
    EXPECT_EQ(buf.front(), 100);
    EXPECT_EQ(buf.back(), 100);
    EXPECT_EQ(buf.size(), 1);

    buf.push(200); // overwrites
    EXPECT_EQ(buf.front(), 200);
    EXPECT_EQ(buf.back(), 200);
    EXPECT_EQ(buf.size(), 1);

    std::vector<int> values;
    for (auto &v : buf)
        values.push_back(v);
    EXPECT_EQ(values, (std::vector<int>{200}));
}

// Heavy stress test
// Heavy stress test with push + iteration timings
static void run_stress_test(size_t CAP) {
    constexpr size_t N = 1'000'000;
    std::cout << "Stress test: Capacity " << CAP << ", pushing " << N << " entries...\n";

    // Dispatch on CAP
    if (CAP == 50) {
        buffer_t<int, 50> buf;

        // Push timing 
        auto t0 = std::chrono::high_resolution_clock::now();
        for (size_t i = 1; i <= N; ++i) buf.push(static_cast<int>(i));
        auto t1 = std::chrono::high_resolution_clock::now();
        auto push_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        std::cout << "  Push done in " << push_ms << " ms\n";

        // Iteration (accumulation) timing 
        size_t acc = 0;
        auto t2 = std::chrono::high_resolution_clock::now();
        for (auto &v : buf) acc += static_cast<size_t>(v);
        auto t3 = std::chrono::high_resolution_clock::now();
        auto iter_ms = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();
        std::cout << "  Iteration (accumulate) done in " << iter_ms << " µs\n";
        EXPECT_GT(acc, 0u);

        // Equality check timing
        auto t4 = std::chrono::high_resolution_clock::now();
        std::vector<int> values;
        for (auto &v : buf) values.push_back(v);
        std::vector<int> expected(50);
        std::iota(expected.rbegin(), expected.rend(), static_cast<int>(N - 50 + 1));
        EXPECT_EQ(values, expected);
        auto t5 = std::chrono::high_resolution_clock::now();
        auto eq_ms = std::chrono::duration_cast<std::chrono::microseconds>(t5 - t4).count();
        std::cout << "  Equality check done in " << eq_ms << " µs\n";
    }

    else if (CAP == 200) {
        buffer_t<int, 200> buf;

        auto t0 = std::chrono::high_resolution_clock::now();
        for (size_t i = 1; i <= N; ++i) buf.push(static_cast<int>(i));
        auto t1 = std::chrono::high_resolution_clock::now();
        auto push_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        std::cout << "  Push done in " << push_ms << " ms\n";

        size_t acc = 0;
        auto t2 = std::chrono::high_resolution_clock::now();
        for (auto &v : buf) acc += static_cast<size_t>(v);
        auto t3 = std::chrono::high_resolution_clock::now();
        auto iter_ms = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();
        std::cout << "  Iteration (accumulate) done in " << iter_ms << " µs\n";
        EXPECT_GT(acc, 0u);

        auto t4 = std::chrono::high_resolution_clock::now();
        std::vector<int> values;
        for (auto &v : buf) values.push_back(v);
        std::vector<int> expected(200);
        std::iota(expected.rbegin(), expected.rend(), static_cast<int>(N - 200 + 1));
        EXPECT_EQ(values, expected);
        auto t5 = std::chrono::high_resolution_clock::now();
        auto eq_ms = std::chrono::duration_cast<std::chrono::microseconds>(t5 - t4).count();
        std::cout << "  Equality check done in " << eq_ms << " µs\n";
    }

    else if (CAP == 2000) {
        buffer_t<int, 2000> buf;

        auto t0 = std::chrono::high_resolution_clock::now();
        for (size_t i = 1; i <= N; ++i) buf.push(static_cast<int>(i));
        auto t1 = std::chrono::high_resolution_clock::now();
        auto push_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        std::cout << "  Push done in " << push_ms << " ms\n";

        size_t acc = 0;
        auto t2 = std::chrono::high_resolution_clock::now();
        for (auto &v : buf) acc += static_cast<size_t>(v);
        auto t3 = std::chrono::high_resolution_clock::now();
        auto iter_ms = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();
        std::cout << "  Iteration (accumulate) done in " << iter_ms << " µs\n";
        EXPECT_GT(acc, 0u);

        auto t4 = std::chrono::high_resolution_clock::now();
        std::vector<int> values;
        for (auto &v : buf) values.push_back(v);
        std::vector<int> expected(2000);
        std::iota(expected.rbegin(), expected.rend(), static_cast<int>(N - 2000 + 1));
        EXPECT_EQ(values, expected);
        auto t5 = std::chrono::high_resolution_clock::now();
        auto eq_ms = std::chrono::duration_cast<std::chrono::microseconds>(t5 - t4).count();
        std::cout << "  Equality check done in " << eq_ms << " µs\n";
    }

    else if (CAP == 20000) {
        buffer_t<int, 20000> buf;

        auto t0 = std::chrono::high_resolution_clock::now();
        for (size_t i = 1; i <= N; ++i) buf.push(static_cast<int>(i));
        auto t1 = std::chrono::high_resolution_clock::now();
        auto push_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        std::cout << "  Push done in " << push_ms << " ms\n";

        size_t acc = 0;
        auto t2 = std::chrono::high_resolution_clock::now();
        for (auto &v : buf) acc += static_cast<size_t>(v);
        auto t3 = std::chrono::high_resolution_clock::now();
        auto iter_ms = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();
        std::cout << "  Iteration (accumulate) done in " << iter_ms << " µs\n";
        EXPECT_GT(acc, 0u);

        auto t4 = std::chrono::high_resolution_clock::now();
        std::vector<int> values;
        for (auto &v : buf) values.push_back(v);
        std::vector<int> expected(20000);
        std::iota(expected.rbegin(), expected.rend(), static_cast<int>(N - 20000 + 1));
        EXPECT_EQ(values, expected);
        auto t5 = std::chrono::high_resolution_clock::now();
        auto eq_ms = std::chrono::duration_cast<std::chrono::microseconds>(t5 - t4).count();
        std::cout << "  Equality check done in " << eq_ms << " µs\n";
    }
}


TEST(RevolvingRecencyBufferTest, Capacity50) { run_stress_test(50); }
TEST(RevolvingRecencyBufferTest, Capacity200) { run_stress_test(200); }
TEST(RevolvingRecencyBufferTest, Capacity2000) { run_stress_test(2000); }
TEST(RevolvingRecencyBufferTest, Capacity20000) { run_stress_test(20000); }
