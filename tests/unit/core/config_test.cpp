#include <gtest/gtest.h>
#include "oculus/core/config.hpp"

using namespace oculus;

TEST(ConfigTest, DefaultValues) {
    auto config = AppConfig::defaults();
    EXPECT_EQ(config.hostname, "oculus");
    EXPECT_EQ(config.api_port, 8080);
    EXPECT_EQ(config.camera.width, 640);
    EXPECT_EQ(config.inference.num_threads, 4);
    EXPECT_EQ(config.rom.analysis_type, "active_rom");
}

TEST(ConfigTest, LoadReturnsDefaults) {
    auto config = AppConfig::load("nonexistent.yaml");
    EXPECT_EQ(config.hostname, "oculus");
}