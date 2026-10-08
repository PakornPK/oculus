#include "oculus/rom/calibrator.hpp"
#include <cmath>
#include <spdlog/spdlog.h>

namespace oculus {

CalibrationData Calibrator::auto_calibrate(const Pose& pose) {
    if (pose.keypoints.size() < 17) return data_;

    const auto& ls = pose.keypoints[static_cast<int>(KeypointIndex::LEFT_SHOULDER)];
    const auto& rs = pose.keypoints[static_cast<int>(KeypointIndex::RIGHT_SHOULDER)];

    float dx = ls.x - rs.x;
    float dy = ls.y - rs.y;
    data_.shoulder_width_pixels = std::sqrt(dx * dx + dy * dy);

    if (data_.shoulder_width_pixels > 0) {
        float estimated_height_px = data_.shoulder_width_pixels * 4.0f;
        float estimated_height_m = 1.7f;
        data_.pixels_per_meter = estimated_height_px / estimated_height_m;
        data_.subject_height_m = estimated_height_m;
    }

    data_.calibrated = true;
    data_.level = CalibrationLevel::AUTO;
    spdlog::info("Auto calibration: shoulder_width={}px, scale={}px/m",
                 data_.shoulder_width_pixels, data_.pixels_per_meter);
    return data_;
}

CalibrationData Calibrator::quick_calibrate(
    const std::vector<Pose>& standing_frames, CalibrationLevel level) {
    if (standing_frames.empty()) return data_;
    data_ = auto_calibrate(standing_frames[0]);
    data_.level = level;
    return data_;
}

CalibrationData Calibrator::standard_calibrate(const Pose& pose, float height_m) {
    if (height_m <= 0) return data_;

    data_ = auto_calibrate(pose);
    if (data_.shoulder_width_pixels > 0) {
        float estimated_height_px = data_.shoulder_width_pixels * 4.0f;
        data_.pixels_per_meter = estimated_height_px / height_m;
        data_.subject_height_m = height_m;
    }

    data_.level = CalibrationLevel::STANDARD;
    spdlog::info("Standard calibration: height={}m, scale={}px/m",
                 height_m, data_.pixels_per_meter);
    return data_;
}

bool Calibrator::is_calibrated() const { return data_.calibrated; }
CalibrationData Calibrator::get_data() const { return data_; }
void Calibrator::reset() { data_ = {}; }

} // namespace oculus