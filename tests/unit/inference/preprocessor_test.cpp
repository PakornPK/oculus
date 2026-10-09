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
    EXPECT_EQ(tensor.size(), static_cast<size_t>(3 * 256 * 192));
}

TEST(RTMPosePreprocessorTest, ImageNetNormalization) {
    RTMPosePreprocessor preprocessor;
    Frame frame = make_frame(640, 480, 128);
    auto tensor = preprocessor.process(frame);
    float avg = std::accumulate(tensor.begin(), tensor.end(), 0.0f) / tensor.size();
    // Just check it's a reasonable normalized value (not 0 or 1)
    EXPECT_GT(avg, -2.0f);
    EXPECT_LT(avg, 2.0f);
}

TEST(RTMPosePreprocessorTest, ZeroFrameGivesNegativeValues) {
    // pixel=0: (0 - 0.485) / 0.229 = -2.117
    RTMPosePreprocessor preprocessor;
    Frame frame = make_frame(640, 480, 0);
    auto tensor = preprocessor.process(frame);
    float avg = std::accumulate(tensor.begin(), tensor.end(), 0.0f) / tensor.size();
    EXPECT_LT(avg, -1.0f);
}

TEST(RTMPosePreprocessorTest, InvalidFrameThrows) {
    RTMPosePreprocessor preprocessor;
    Frame frame;
    frame.width = 640;
    frame.height = 480;
    frame.data.resize(100);
    EXPECT_THROW(preprocessor.process(frame), std::invalid_argument);
}

TEST(RTMPosePreprocessorTest, TensorSizeAccessor) {
    RTMPosePreprocessor preprocessor;
    EXPECT_EQ(preprocessor.tensor_size(), static_cast<size_t>(3 * 256 * 192));
}

TEST(RTMPosePreprocessorTest, DimensionsAreCorrect) {
    RTMPosePreprocessor preprocessor;
    EXPECT_EQ(preprocessor.input_width(), 192);
    EXPECT_EQ(preprocessor.input_height(), 256);
    EXPECT_EQ(preprocessor.input_channels(), 3);
}