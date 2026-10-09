#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <cmath>

namespace oculus {

using MeasurementMap = std::unordered_map<std::string, float>;

class MeasurementFormula {
public:
    static float total_arc(float flexion, float extension);
    static float bilateral_comparison(float affected, float unaffected);
    static float rom_percentage(float measured, float normal);
    static float symmetry_index(float left, float right);
    static float movement_velocity(float angle_start, float angle_end, float time_sec);

    static MeasurementMap compute_all(const MeasurementMap& raw_angles);
};

} // namespace oculus