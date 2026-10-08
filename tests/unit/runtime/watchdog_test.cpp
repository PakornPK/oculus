#include <gtest/gtest.h>
#include "oculus/runtime/watchdog.hpp"

using namespace oculus;

TEST(WatchdogTest, StartStop) {
    Watchdog wd;
    wd.start(std::chrono::seconds(5));
    EXPECT_TRUE(wd.is_alive());
    wd.stop();
}

TEST(WatchdogTest, FeedKeepsAlive) {
    Watchdog wd;
    wd.start(std::chrono::seconds(2));
    wd.feed();
    EXPECT_TRUE(wd.is_alive());
    wd.stop();
}