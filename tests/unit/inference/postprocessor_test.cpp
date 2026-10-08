#include <gtest/gtest.h>
#include "oculus/inference/models/rtmpose/rtmpose_postprocessor.hpp"
#include "oculus/core/pose.hpp"

#include <cmath>
#include <vector>

using namespace oculus;

// Create a synthetic heatmap with a peak at a known location.
static std::vector<float> make_heatmap_with_peak(
    int keypoint_index, int peak_x, int peak_y, float peak_value = 1.0f) {
    const int size = 17 * 48 * 64;
    std::vector<float> heatmap(size, 0.0f);

    int offset = keypoint_index * 48 * 64;
    heatmap[offset + peak_y * 64 + peak_x] = peak_value;

    return heatmap;
}

TEST(RTMPosePostprocessorTest, ReturnsCorrectNumberOfKeypoints) {
    RTMPosePostprocessor postprocessor;
    auto heatmap = make_heatmap_with_peak(0, 32, 24);

    auto result = postprocessor.process(heatmap);

    ASSERT_EQ(result.poses.size(), 1u);
    EXPECT_EQ(result.poses[0].keypoints.size(), 17u);
}

TEST(RTMPosePostprocessorTest, KeypointAtPeakLocation) {
    RTMPosePostprocessor postprocessor;
    int peak_x = 32, peak_y = 24;
    auto heatmap = make_heatmap_with_peak(0, peak_x, peak_y);

    auto result = postprocessor.process(heatmap);
    auto& kp = result.poses[0].keypoints[0];

    // Image coordinates: peak_x * (256/64) = 128, peak_y * (192/48) = 96
    EXPECT_FLOAT_EQ(kp.x, 128.0f);
    EXPECT_FLOAT_EQ(kp.y, 96.0f);
    EXPECT_FLOAT_EQ(kp.confidence, 1.0f);
}

TEST(RTMPosePostprocessorTest, MultipleKeypointsDifferentPeaks) {
    RTMPosePostprocessor postprocessor;
    const int size = 17 * 48 * 64;
    std::vector<float> heatmap(size, 0.0f);

    // Place peaks for keypoint 0 and keypoint 5
    heatmap[0 * 48 * 64 + 10 * 64 + 20] = 0.9f;   // kp0: (20,10)
    heatmap[5 * 48 * 64 + 30 * 64 + 40] = 0.8f;   // kp5: (40,30)

    auto result = postprocessor.process(heatmap);
    auto& kp0 = result.poses[0].keypoints[0];
    auto& kp5 = result.poses[0].keypoints[5];

    EXPECT_FLOAT_EQ(kp0.x, 20.0f * 256.0f / 64.0f); // = 80.0
    EXPECT_FLOAT_EQ(kp0.y, 10.0f * 192.0f / 48.0f); // = 40.0
    EXPECT_FLOAT_EQ(kp0.confidence, 0.9f);

    EXPECT_FLOAT_EQ(kp5.x, 40.0f * 256.0f / 64.0f); // = 160.0
    EXPECT_FLOAT_EQ(kp5.y, 30.0f * 192.0f / 48.0f); // = 120.0
    EXPECT_FLOAT_EQ(kp5.confidence, 0.8f);
}

TEST(RTMPosePostprocessorTest, TimestampIsPreserved) {
    RTMPosePostprocessor postprocessor;
    auto heatmap = make_heatmap_with_peak(0, 32, 24);

    auto result = postprocessor.process(heatmap, 12345);

    EXPECT_EQ(result.timestamp, 12345);
}

TEST(RTMPosePostprocessorTest, AllZeroHeatmapGivesZeroConfidence) {
    RTMPosePostprocessor postprocessor;
    std::vector<float> heatmap(17 * 48 * 64, 0.0f);

    auto result = postprocessor.process(heatmap);

    ASSERT_EQ(result.poses.size(), 1u);
    for (int k = 0; k < 17; ++k) {
        EXPECT_FLOAT_EQ(result.poses[0].keypoints[k].confidence, 0.0f);
    }
}

TEST(RTMPosePostprocessorTest, HeatmapTooSmallThrows) {
    RTMPosePostprocessor postprocessor;
    std::vector<float> heatmap(10, 0.0f);

    EXPECT_THROW(postprocessor.process(heatmap), std::invalid_argument);
}

TEST(RTMPosePostprocessorTest, DimensionsAreCorrect) {
    RTMPosePostprocessor postprocessor;
    EXPECT_EQ(postprocessor.heatmap_width(), 64);
    EXPECT_EQ(postprocessor.heatmap_height(), 48);
    EXPECT_EQ(postprocessor.num_keypoints(), 17);
}