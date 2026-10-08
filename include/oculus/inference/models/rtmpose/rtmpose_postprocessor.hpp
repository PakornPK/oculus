#pragma once

#include "oculus/core/pose.hpp"
#include <vector>

namespace oculus {

class RTMPosePostprocessor {
public:
    static constexpr int INPUT_WIDTH = 256;
    static constexpr int INPUT_HEIGHT = 192;
    static constexpr int HEATMAP_WIDTH = 64;
    static constexpr int HEATMAP_HEIGHT = 48;
    static constexpr int NUM_KEYPOINTS = KEYPOINT_COUNT; // 17

    // Extract poses from a heatmap tensor (17 x 48 x 64).
    // Returns a PoseResult with up to max_poses poses.
    PoseResult process(const std::vector<float>& heatmap,
                       int64_t timestamp = 0,
                       int max_poses = 1) const;

    int heatmap_width() const { return HEATMAP_WIDTH; }
    int heatmap_height() const { return HEATMAP_HEIGHT; }
    int num_keypoints() const { return NUM_KEYPOINTS; }

private:
    // Find the index of the maximum value in a 2D heatmap slice.
    // Returns (x, y) coordinates in heatmap space and the max value.
    struct ArgmaxResult {
        int x = 0;
        int y = 0;
        float value = 0.0f;
    };
    ArgmaxResult argmax_2d(const float* data, int width, int height) const;

    // Convert heatmap coordinates to input image coordinates (256x192).
    void heatmap_to_image(float& x, float& y) const;
};

} // namespace oculus