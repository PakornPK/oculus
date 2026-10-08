#include <gtest/gtest.h>
#include "oculus/inference/models/rtmpose/rtmpose_preprocessor.hpp"
#include "oculus/core/frame.hpp"

#include <numeric>

using namespace oculus;

static Frame make_frame(int width = 640, int height = 480, uint8_t fill = 128) {
    Frame frame;
    frame.width = width;
    frame.height = height;
    frame.format = PixelFormat::RGB;
    frame.data.resize(static_cast<size_t>(width) * height * 3, fill);
    return frame;
}

TEST(RTMPosePreprocessorTest, OutputShapeIsCorrect) {
    RTMPosePreprocessor preprocessor;
    Frame frame = make_frame();

    auto tensor = preprocessor.process(frame);

    EXPECT_EQ(tensor.size(),
              static_cast<size_t>(1 * 3 * 192 * 256));
}

TEST(RTMPosePreprocessorTest, NormalizedToZeroOne) {
    RTMPosePreprocessor preprocessor;
    Frame frame = make_frame(640, 480, 255);

    auto tensor = preprocessor.process(frame);

    for (float val : tensor) {
        EXPECT_GE(val, 0.0f);
        EXPECT_LE(val, 1.0f);
    }
}

TEST(RTMPosePreprocessorTest, ZeroFrameGivesZeros) {
    RTMPosePreprocessor preprocessor;
    Frame frame = make_frame(640, 480, 0);

    auto tensor = preprocessor.process(frame);

    for (float val : tensor) {
        EXPECT_FLOAT_EQ(val, 0.0f);
    }
}

TEST(RTMPosePreprocessorTest, MaxFrameGivesOnes) {
    RTMPosePreprocessor preprocessor;
    Frame frame = make_frame(640, 480, 255);

    auto tensor = preprocessor.process(frame);

    for (float val : tensor) {
        EXPECT_FLOAT_EQ(val, 1.0f);
    }
}

TEST(RTMPosePreprocessorTest, InvalidFrameThrows) {
    RTMPosePreprocessor preprocessor;
    Frame frame;
    frame.width = 640;
    frame.height = 480;
    frame.data.resize(100); // mismatch

    EXPECT_THROW(preprocessor.process(frame), std::invalid_argument);
}

TEST(RTMPosePreprocessorTest, TensorSizeAccessor) {
    RTMPosePreprocessor preprocessor;
    EXPECT_EQ(preprocessor.tensor_size(),
              static_cast<size_t>(3 * 192 * 256));
}

TEST(RTMPosePreprocessorTest, DimensionsAreCorrect) {
    RTMPosePreprocessor preprocessor;
    EXPECT_EQ(preprocessor.input_width(), 256);
    EXPECT_EQ(preprocessor.input_height(), 192);
    EXPECT_EQ(preprocessor.input_channels(), 3);
}