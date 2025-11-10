#include <gtest/gtest.h>
#include <thread>
#include <thread>
#include "mcds/circular_buffer.hpp"

using namespace btc_stream::streamer::buffers;

TEST(CircularBuffer, BasicPushPop)
{
    circular_buffer<int, 8> circ_buff;
    EXPECT_EQ(circ_buff.size(), 0);

    // Push into empty
    EXPECT_TRUE(circ_buff.emplace(42));
    EXPECT_EQ(circ_buff.size(), 1);

    auto out = circ_buff.pop();
    // Pop and expect
    EXPECT_TRUE(out);
    EXPECT_EQ(circ_buff.size(), 0);
    EXPECT_EQ(*out, 42);
}

TEST(CircularBuffer, FifoOrdering)
{
    circular_buffer<int, 9> circ_buff;
    for (int i = 0; i < 8; ++i)
        circ_buff.emplace(i);
    for (int i = 0; i < 8; ++i)
    {
        auto out = circ_buff.pop();
        EXPECT_TRUE(out);
        EXPECT_EQ(*out, i);
    }
}

TEST(CircularBuffer, FullCondition)
{
    circular_buffer<int, 4> circ_buff;
    EXPECT_TRUE(circ_buff.emplace(1));
    EXPECT_TRUE(circ_buff.emplace(1));
    EXPECT_TRUE(circ_buff.emplace(1));
    EXPECT_FALSE(circ_buff.emplace(1)); // buffer full
}

TEST(CircularBuffer, EmptyCondition)
{
    circular_buffer<int, 4> circ_buff;
    EXPECT_FALSE(circ_buff.pop()); // empty
}

TEST(CircularBuffer, WrapAround)
{
    circular_buffer<int, 4> circ_buff;
    EXPECT_TRUE(circ_buff.emplace(1));
    EXPECT_TRUE(circ_buff.emplace(2));
    EXPECT_TRUE(circ_buff.emplace(3));
    EXPECT_FALSE(circ_buff.emplace(4)); // buffer full
    auto out = circ_buff.pop();
    EXPECT_TRUE(out);                  // remove 1
    EXPECT_TRUE(circ_buff.emplace(5)); // should wrap
    out = circ_buff.pop();
    EXPECT_TRUE(out); // remove 2
    EXPECT_EQ(*out, 2);
}

struct Tracker
{
    static inline int ctor_count = 0;
    static inline int dtor_count = 0;
    Tracker() { ctor_count++; }
    ~Tracker()
    {
        dtor_count++;
    }
};

TEST(CircularBuffer, DestructorCleanUp)
{
    {
        circular_buffer<Tracker, 4> circ_buff;
        circ_buff.emplace();
        circ_buff.emplace();
        EXPECT_EQ(Tracker::ctor_count, 2); // count constructions
    }
    EXPECT_EQ(Tracker::dtor_count, 2); // check destruction, one extra for temp in destructor
}

TEST(CircularBuffer, ThreadedProducerConsumer)
{
    constexpr size_t N = 1'000'000;
    circular_buffer<int, 1024> circ_buff;

    std::vector<int> consumed;
    consumed.reserve(N);

    // Prod Con threads
    std::thread producer([&]
                         {
        for(int i = 0; i < static_cast<int>(N); ++i)
        {
            while(!circ_buff.emplace(i))
            {
                std::this_thread::yield(); // spin until finished emplacing
            }
        } });

    std::thread consumer([&]
                         {
        for(int i = 0; i < static_cast<int>(N); ++i)
        {
            std::optional<int> out;
            // Spin until all popped
            while(!(out = circ_buff.pop()))
            {
                std::this_thread::yield();
            }
            consumed.push_back(*out);

        } });

    // Wait until finished
    producer.join();
    consumer.join();

    // Check all consumed in order
    EXPECT_EQ(consumed.size(), N);
    for (size_t i = 0; i < N; ++i)
    {
        EXPECT_EQ(consumed[i], i);
    }
}