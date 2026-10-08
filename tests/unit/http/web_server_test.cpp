#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "oculus/http/web_server.hpp"

using namespace oculus;

TEST(WebServerTest, CreatesAndStops) {
    WebConfig config;
    config.api_port = 18080;
    WebServer server(config);
    EXPECT_FALSE(server.is_running());
}

TEST(WebServerTest, StartStopLifecycle) {
    WebConfig config;
    config.api_port = 18081;
    WebServer server(config);
    server.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(server.is_running());
    server.stop();
    EXPECT_FALSE(server.is_running());
}