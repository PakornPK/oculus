#include "oculus/rom/joint_angle_calculator.hpp"
#include <stdexcept>

namespace oculus {

float JointAngleCalculator::dot(const Keypoint& a, const Keypoint& b) {
    return a.x * b.x + a.y * b.y;
}

float JointAngleCalculator::magnitude(const Keypoint& v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

Keypoint JointAngleCalculator::subtract(const Keypoint& a, const Keypoint& b) {
    return {a.x - b.x, a.y - b.y, 0.0f};
}

float JointAngleCalculator::angle_from_vertical(
    const Keypoint& shoulder, const Keypoint& elbow, const Keypoint& hip) {

    auto upper_arm = subtract(elbow, shoulder);
    auto reference = subtract(hip, shoulder);

    float arm_mag = magnitude(upper_arm);
    float ref_mag = magnitude(reference);
    if (arm_mag < 1e-6f || ref_mag < 1e-6f) return 0.0f;

    float cos_angle = dot(upper_arm, reference) / (arm_mag * ref_mag);
    cos_angle = std::max(-1.0f, std::min(1.0f, cos_angle));
    float angle_rad = std::acos(cos_angle);
    float angle_deg = angle_rad * 180.0f / M_PI;

    return angle_deg;
}

float JointAngleCalculator::forearm_orientation(
    const Keypoint& wrist, const Keypoint& elbow, const Keypoint& shoulder) {

    auto forearm = subtract(wrist, elbow);
    auto upper_arm = subtract(elbow, shoulder);

    float arm_mag = magnitude(upper_arm);
    if (arm_mag < 1e-6f) return 0.0f;

    Keypoint ref = {-upper_arm.y, upper_arm.x, 0.0f};

    float forearm_mag = magnitude(forearm);
    if (forearm_mag < 1e-6f) return 0.0f;

    float cos_angle = dot(forearm, ref) / (forearm_mag * magnitude(ref));
    cos_angle = std::max(-1.0f, std::min(1.0f, cos_angle));
    float angle_deg = std::acos(cos_angle) * 180.0f / M_PI;

    float cross = forearm.x * ref.y - forearm.y * ref.x;
    if (cross < 0) angle_deg = -angle_deg;

    return angle_deg;
}

float JointAngleCalculator::vertical_distance(const Keypoint& a, const Keypoint& b) {
    return std::abs(a.y - b.y);
}

float JointAngleCalculator::horizontal_offset(const Keypoint& a, const Keypoint& b) {
    return a.x - b.x;
}

float JointAngleCalculator::calculate_angle(
    const Pose& pose, MovementType movement, Side side) {

    int shoulder_idx, elbow_idx, hip_idx, wrist_idx, ear_idx;

    if (side == Side::LEFT) {
        shoulder_idx = static_cast<int>(KeypointIndex::LEFT_SHOULDER);
        elbow_idx = static_cast<int>(KeypointIndex::LEFT_ELBOW);
        hip_idx = static_cast<int>(KeypointIndex::LEFT_HIP);
        wrist_idx = static_cast<int>(KeypointIndex::LEFT_WRIST);
        ear_idx = static_cast<int>(KeypointIndex::LEFT_EAR);
    } else {
        shoulder_idx = static_cast<int>(KeypointIndex::RIGHT_SHOULDER);
        elbow_idx = static_cast<int>(KeypointIndex::RIGHT_ELBOW);
        hip_idx = static_cast<int>(KeypointIndex::RIGHT_HIP);
        wrist_idx = static_cast<int>(KeypointIndex::RIGHT_WRIST);
        ear_idx = static_cast<int>(KeypointIndex::RIGHT_EAR);
    }

    int max_idx = ear_idx;
    if (max_idx >= static_cast<int>(pose.keypoints.size())) {
        return 0.0f;
    }

    const auto& shoulder = pose.keypoints[shoulder_idx];
    const auto& elbow = pose.keypoints[elbow_idx];
    const auto& hip = pose.keypoints[hip_idx];
    const auto& wrist = pose.keypoints[wrist_idx];
    const auto& ear = pose.keypoints[ear_idx];

    switch (movement) {
        case MovementType::ABDUCTION:
        case MovementType::FORWARD_FLEXION:
        case MovementType::EXTENSION:
        case MovementType::ADDUCTION:
        case MovementType::HORIZONTAL_ADDDUCTION:
            return angle_from_vertical(shoulder, elbow, hip);

        case MovementType::EXTERNAL_ROTATION:
        case MovementType::INTERNAL_ROTATION:
            return forearm_orientation(wrist, elbow, shoulder);

        case MovementType::SCAPULAR_PROTRACTION:
        case MovementType::SCAPULAR_RETRACTION:
            return horizontal_offset(shoulder, ear);

        case MovementType::SHOULDER_ELEVATION:
        case MovementType::SHOULDER_DEPRESSION:
            return vertical_distance(shoulder, ear);
    }

    return 0.0f;
}

} // namespace oculus