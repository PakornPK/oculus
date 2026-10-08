#pragma once

#include "oculus/core/pose.hpp"

namespace oculus {

enum class CalibrationLevel {
    AUTO,
    QUICK,
    STANDARD,
    FULL
};

struct CalibrationData {
    float pixels_per_meter = 0.0f;
    float subject_height_m = 0.0f;
    float shoulder_width_pixels = 0.0f;
    bool calibrated = false;
    CalibrationLevel level = CalibrationLevel::AUTO;
};

class Calibrator {
public:
    CalibrationData auto_calibrate(const Pose& pose);
    CalibrationData quick_calibrate(const std::vector<Pose>& standing_frames,
                                     CalibrationLevel level = CalibrationLevel::QUICK);
    CalibrationData standard_calibrate(const Pose& pose, float height_m);

    bool is_calibrated() const;
    CalibrationData get_data() const;
    void reset();

private:
    CalibrationData data_;
};

} // namespace oculus