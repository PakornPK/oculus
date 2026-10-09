#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <cmath>

namespace oculus {

using MeasurementMap = std::unordered_map<std::string, float>;

class MeasurementFormula {
public:
    // Clinical A-B notation: ROM = B - A
    static float ab_notation_rom(float a, float b);
    static float total_arc(float flexion, float extension);
    static float bilateral_comparison(float affected, float unaffected);
    static float rom_percentage(float measured, float normal);
    static float symmetry_index(float left, float right);
    static float movement_velocity(float angle_start, float angle_end, float time_sec);

    // Clinical normal ROM values (Norkin & White)
    static float normal_flexion() { return 180.0f; }
    static float normal_extension() { return 60.0f; }
    static float normal_abduction() { return 180.0f; }
    static float normal_horizontal_adduction() { return 130.0f; }
    static float normal_external_rotation() { return 90.0f; }
    static float normal_internal_rotation() { return 70.0f; }

    static MeasurementMap compute_all(const MeasurementMap& raw_angles);
};

} // namespace oculus