#pragma once

#include "oculus/core/pose.hpp"
#include <cmath>

namespace oculus {

enum class MovementType {
    ABDUCTION,
    FORWARD_FLEXION,
    EXTENSION,
    EXTERNAL_ROTATION,
    INTERNAL_ROTATION,
    ADDUCTION,
    HORIZONTAL_ADDDUCTION,
    SCAPULAR_PROTRACTION,
    SCAPULAR_RETRACTION,
    SHOULDER_ELEVATION,
    SHOULDER_DEPRESSION
};

enum class Side {
    LEFT,
    RIGHT
};

class JointAngleCalculator {
public:
    float calculate_angle(const Pose& pose, MovementType movement, Side side);

    static float angle_from_vertical(const Keypoint& shoulder,
                                     const Keypoint& elbow,
                                     const Keypoint& hip);

    static float forearm_orientation(const Keypoint& wrist,
                                     const Keypoint& elbow,
                                     const Keypoint& shoulder);

    static float vertical_distance(const Keypoint& a, const Keypoint& b);
    static float horizontal_offset(const Keypoint& a, const Keypoint& b);

private:
    static float dot(const Keypoint& a, const Keypoint& b);
    static float magnitude(const Keypoint& v);
    static Keypoint subtract(const Keypoint& a, const Keypoint& b);
};

} // namespace oculus