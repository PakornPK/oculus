#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace oculus {

struct Keypoint {
    float x = 0.0f;
    float y = 0.0f;
    float confidence = 0.0f;
};

struct Pose {
    std::vector<Keypoint> keypoints;
    float confidence = 0.0f;
};

struct PoseResult {
    std::vector<Pose> poses;
    int64_t timestamp = 0;
};

enum class KeypointIndex : int {
    NOSE = 0,
    LEFT_EYE = 1,
    RIGHT_EYE = 2,
    LEFT_EAR = 3,
    RIGHT_EAR = 4,
    LEFT_SHOULDER = 5,
    RIGHT_SHOULDER = 6,
    LEFT_ELBOW = 7,
    RIGHT_ELBOW = 8,
    LEFT_WRIST = 9,
    RIGHT_WRIST = 10,
    LEFT_HIP = 11,
    RIGHT_HIP = 12,
    LEFT_KNEE = 13,
    RIGHT_KNEE = 14,
    LEFT_ANKLE = 15,
    RIGHT_ANKLE = 16
};

inline constexpr int KEYPOINT_COUNT = 17;

} // namespace oculus