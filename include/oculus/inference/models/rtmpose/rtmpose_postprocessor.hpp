#pragma once

#include "oculus/core/pose.hpp"
#include <vector>

namespace oculus {

class RTMPosePostprocessor {
public:
    static constexpr int INPUT_WIDTH = 192;
    static constexpr int INPUT_HEIGHT = 256;
    static constexpr int SIMCC_X_SIZE = 384;  // 2x input width
    static constexpr int SIMCC_Y_SIZE = 512;  // 2x input height
    static constexpr int NUM_KEYPOINTS = KEYPOINT_COUNT; // 17

    // Extract poses from SimCC output (simcc_x [1,17,384], simcc_y [1,17,512]).
    PoseResult process_simcc(const std::vector<float>& simcc_x,
                             const std::vector<float>& simcc_y,
                             int64_t timestamp = 0) const;

    // Legacy heatmap interface (for compatibility).
    PoseResult process(const std::vector<float>& heatmap,
                       int64_t timestamp = 0,
                       int max_poses = 1) const;

    int num_keypoints() const { return NUM_KEYPOINTS; }

private:
    int argmax_1d(const float* data, int size) const;
    float refine_coordinate(const float* data, int size, int peak_idx) const;
};

} // namespace oculus