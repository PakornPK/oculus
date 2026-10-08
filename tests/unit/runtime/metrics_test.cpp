#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "oculus/runtime/metrics.hpp"

using namespace oculus;

TEST(MetricsTest, RecordAndRetrieveLatency) {
    MetricsCollector metrics;
    metrics.record_latency("inference", 10.0);
    metrics.record_latency("inference", 20.0);

    EXPECT_NEAR(metrics.get_avg_latency("inference"), 15.0, 0.1);
}

TEST(MetricsTest, UnknownLatencyReturnsZero) {
    MetricsCollector metrics;
    EXPECT_DOUBLE_EQ(metrics.get_avg_latency("unknown"), 0.0);
}

TEST(MetricsTest, FPS) {
    MetricsCollector metrics;
    metrics.record_fps(30.0);
    EXPECT_DOUBLE_EQ(metrics.get_fps(), 30.0);
}

TEST(MetricsTest, ScopedTimer) {
    MetricsCollector metrics;
    {
        ScopedTimer timer(metrics, "test_op");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_GT(metrics.get_avg_latency("test_op"), 0.0);
}

TEST(MetricsTest, Reset) {
    MetricsCollector metrics;
    metrics.record_latency("test", 10.0);
    metrics.record_fps(30.0);
    metrics.reset();

    EXPECT_DOUBLE_EQ(metrics.get_avg_latency("test"), 0.0);
    EXPECT_DOUBLE_EQ(metrics.get_fps(), 0.0);
}