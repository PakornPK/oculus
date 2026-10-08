#include <gtest/gtest.h>
#include "oculus/core/hardware_detector.hpp"

using namespace oculus;

TEST(HardwareDetectorTest, DetectsArchitecture) {
    auto info = HardwareDetector::detect();
    EXPECT_FALSE(info.arch.empty());
    EXPECT_TRUE(info.arch == "x86_64" || info.arch == "aarch64");
}

TEST(HardwareDetectorTest, DetectsOS) {
    auto info = HardwareDetector::detect();
    EXPECT_FALSE(info.os.empty());
}

TEST(HardwareDetectorTest, DetectsCPU) {
    auto info = HardwareDetector::detect();
    EXPECT_GT(info.cpu_cores, 0);
    EXPECT_FALSE(info.cpu_model.empty());
}

TEST(HardwareDetectorTest, DetectsMemory) {
    auto info = HardwareDetector::detect();
    EXPECT_GT(info.total_memory_mb, 0);
}

TEST(HardwareDetectorTest, HasAtLeastCPUBackend) {
    auto info = HardwareDetector::detect();
    EXPECT_FALSE(info.available_backends.empty());

    bool has_cpu = false;
    for (const auto& b : info.available_backends) {
        if (b == "cpu") has_cpu = true;
    }
    EXPECT_TRUE(has_cpu);
}

TEST(HardwareDetectorTest, SelectedBackendIsNotEmpty) {
    auto info = HardwareDetector::detect();
    EXPECT_FALSE(info.selected_backend.empty());
}

TEST(HardwareDetectorTest, DiagnoseReturnsResults) {
    auto results = HardwareDetector::diagnose();
    EXPECT_GE(results.size(), 3u);

    for (const auto& r : results) {
        EXPECT_FALSE(r.component.empty());
        EXPECT_FALSE(r.message.empty());
    }
}