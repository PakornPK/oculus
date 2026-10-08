#include "oculus/inference/models/rtmpose/rtmpose_postprocessor.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace oculus {

PoseResult RTMPosePostprocessor::process(const std::vector<float>& heatmap,
                                         int64_t timestamp,
                                         int max_poses) const {
    const size_t expected_size = static_cast<size_t>(NUM_KEYPOINTS) *
                                  HEATMAP_HEIGHT * HEATMAP_WIDTH;
    if (heatmap.size() < expected_size) {
        throw std::invalid_argument("Heatmap too small: expected " +
                                    std::to_string(expected_size) + " got " +
                                    std::to_string(heatmap.size()));
    }

    PoseResult result;
    result.timestamp = timestamp;

    // Extract one pose from heatmaps using per-channel argmax
    Pose pose;
    pose.keypoints.resize(NUM_KEYPOINTS);
    float total_confidence = 0.0f;

    for (int k = 0; k < NUM_KEYPOINTS; ++k) {
        const float* channel_data = heatmap.data() +
            static_cast<size_t>(k) * HEATMAP_HEIGHT * HEATMAP_WIDTH;

        ArgmaxResult am = argmax_2d(channel_data, HEATMAP_WIDTH, HEATMAP_HEIGHT);

        Keypoint kp;
        kp.confidence = am.value;
        heatmap_to_image(kp.x, kp.y);
        // Re-assign after conversion
        kp.x = static_cast<float>(am.x) * INPUT_WIDTH / HEATMAP_WIDTH;
        kp.y = static_cast<float>(am.y) * INPUT_HEIGHT / HEATMAP_HEIGHT;
        kp.confidence = am.value;

        pose.keypoints[k] = kp;
        total_confidence += am.value;
    }

    pose.confidence = total_confidence / NUM_KEYPOINTS;
    result.poses.push_back(pose);

    return result;
}

RTMPosePostprocessor::ArgmaxResult RTMPosePostprocessor::argmax_2d(
    const float* data, int width, int height) const {
    ArgmaxResult result;
    result.value = -std::numeric_limits<float>::max();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float val = data[y * width + x];
            if (val > result.value) {
                result.value = val;
                result.x = x;
                result.y = y;
            }
        }
    }

    return result;
}

void RTMPosePostprocessor::heatmap_to_image(float& x, float& y) const {
    // Scaling from heatmap coordinates to input image coordinates
    // is handled inline during process(); this helper is reserved
    // for future sub-pixel refinement (Taylor expansion).
}

} // namespace oculus