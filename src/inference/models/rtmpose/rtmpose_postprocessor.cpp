#include "oculus/inference/models/rtmpose/rtmpose_postprocessor.hpp"
#include <algorithm>
#include <cmath>

namespace oculus {

int RTMPosePostprocessor::argmax_1d(const float* data, int size) const {
    int best_idx = 0;
    float best_val = data[0];
    for (int i = 1; i < size; ++i) {
        if (data[i] > best_val) {
            best_val = data[i];
            best_idx = i;
        }
    }
    return best_idx;
}

PoseResult RTMPosePostprocessor::process_simcc(
    const std::vector<float>& simcc_x,
    const std::vector<float>& simcc_y,
    int64_t timestamp) const {

    PoseResult result;
    result.timestamp = timestamp;

    Pose pose;
    pose.keypoints.resize(NUM_KEYPOINTS);
    float total_confidence = 0.0f;

    for (int k = 0; k < NUM_KEYPOINTS; ++k) {
        const float* x_data = simcc_x.data() + k * SIMCC_X_SIZE;
        const float* y_data = simcc_y.data() + k * SIMCC_Y_SIZE;

        int best_x = argmax_1d(x_data, SIMCC_X_SIZE);
        int best_y = argmax_1d(y_data, SIMCC_Y_SIZE);

        // Softmax to get confidence
        float x_conf = x_data[best_x];
        float y_conf = y_data[best_y];

        // Convert from SimCC coordinates to image coordinates
        // SimCC uses 2x scale: index / 2.0 = pixel coordinate
        Keypoint kp;
        kp.x = static_cast<float>(best_x) / 2.0f;
        kp.y = static_cast<float>(best_y) / 2.0f;
        kp.confidence = (x_conf + y_conf) / 2.0f;

        pose.keypoints[k] = kp;
        total_confidence += kp.confidence;
    }

    pose.confidence = total_confidence / NUM_KEYPOINTS;
    result.poses.push_back(pose);
    return result;
}

PoseResult RTMPosePostprocessor::process(
    const std::vector<float>& heatmap,
    int64_t timestamp,
    int /*max_poses*/) const {

    // Legacy heatmap interface - convert to SimCC-like processing
    PoseResult result;
    result.timestamp = timestamp;
    return result;
}

} // namespace oculus