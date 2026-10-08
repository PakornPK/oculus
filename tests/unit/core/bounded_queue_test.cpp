#include <gtest/gtest.h>
#include "oculus/core/bounded_queue.hpp"
#include <string>
#include <thread>

using namespace oculus;

TEST(BoundedQueueTest, PushAndPop) {
    BoundedQueue<int> queue(3);
    EXPECT_TRUE(queue.try_push(1));
    EXPECT_TRUE(queue.try_push(2));
    EXPECT_EQ(queue.size(), 2u);

    auto val = queue.try_pop();
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 1);
}

TEST(BoundedQueueTest, RejectsWhenFull) {
    BoundedQueue<int> queue(2);
    EXPECT_TRUE(queue.try_push(1));
    EXPECT_TRUE(queue.try_push(2));
    EXPECT_FALSE(queue.try_push(3));
    EXPECT_EQ(queue.size(), 2u);
}

TEST(BoundedQueueTest, OverwriteWhenFull) {
    BoundedQueue<int> queue(2);
    queue.push_overwrite(1);
    queue.push_overwrite(2);
    queue.push_overwrite(3);

    EXPECT_EQ(queue.size(), 2u);
    auto val = queue.try_pop();
    EXPECT_EQ(*val, 2);
}

TEST(BoundedQueueTest, EmptyQueueReturnsNullopt) {
    BoundedQueue<int> queue(3);
    auto val = queue.try_pop();
    EXPECT_FALSE(val.has_value());
}

TEST(BoundedQueueTest, Clear) {
    BoundedQueue<int> queue(3);
    queue.try_push(1);
    queue.try_push(2);
    queue.clear();
    EXPECT_TRUE(queue.empty());
}

TEST(BoundedQueueTest, WaitPopTimeout) {
    BoundedQueue<int> queue(3);
    auto val = queue.wait_pop(std::chrono::milliseconds(50));
    EXPECT_FALSE(val.has_value());
}

TEST(BoundedQueueTest, WaitPopSuccess) {
    BoundedQueue<int> queue(3);
    queue.try_push(42);
    auto val = queue.wait_pop(std::chrono::milliseconds(100));
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 42);
}