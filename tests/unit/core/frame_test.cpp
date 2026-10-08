#include <gtest/gtest.h>
#include "oculus/core/frame.hpp"

using namespace oculus;

TEST(FrameTest, CreatesValidRGBFrame) {
    Frame frame;
    frame.width = 640;
    frame.height = 480;
    frame.format = PixelFormat::RGB;
    frame.data.resize(640 * 480 * 3);

    EXPECT_TRUE(frame.is_valid());
    EXPECT_EQ(frame.channels(), 3);
    EXPECT_EQ(frame.expected_size(), 640u * 480 * 3);
}

TEST(FrameTest, CreatesValidGrayFrame) {
    Frame frame;
    frame.width = 320;
    frame.height = 240;
    frame.format = PixelFormat::GRAY;
    frame.data.resize(320 * 240);

    EXPECT_TRUE(frame.is_valid());
    EXPECT_EQ(frame.channels(), 1);
}

TEST(FrameTest, InvalidWhenZeroDimensions) {
    Frame frame;
    frame.width = 0;
    frame.height = 0;

    EXPECT_FALSE(frame.is_valid());
}

TEST(FrameTest, InvalidWhenDataSizeMismatch) {
    Frame frame;
    frame.width = 640;
    frame.height = 480;
    frame.format = PixelFormat::RGB;
    frame.data.resize(100);

    EXPECT_FALSE(frame.is_valid());
}

TEST(FrameTest, DefaultFrameIsInvalid) {
    Frame frame;
    EXPECT_FALSE(frame.is_valid());
}