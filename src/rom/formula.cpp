#include "oculus/rom/formula.hpp"
#include <algorithm>

namespace oculus {

float MeasurementFormula::total_arc(float flexion, float extension) {
    return flexion + extension;
}

float MeasurementFormula::bilateral_comparison(float affected, float unaffected) {
    if (unaffected <= 0.0f) return 0.0f;
    return (affected / unaffected) * 100.0f;
}

float MeasurementFormula::rom_percentage(float measured, float normal) {
    if (normal <= 0.0f) return 0.0f;
    return (measured / normal) * 100.0f;
}

float MeasurementFormula::symmetry_index(float left, float right) {
    float max_val = std::max(left, right);
    float min_val = std::min(left, right);
    if (max_val <= 0.0f) return 100.0f;
    return (min_val / max_val) * 100.0f;
}

float MeasurementFormula::movement_velocity(float angle_start, float angle_end, float time_sec) {
    if (time_sec <= 0.0f) return 0.0f;
    return std::abs(angle_end - angle_start) / time_sec;
}

MeasurementMap MeasurementFormula::compute_all(const MeasurementMap& raw_angles) {
    MeasurementMap results = raw_angles;

    auto get = [&](const std::string& key) -> float {
        auto it = raw_angles.find(key);
        return (it != raw_angles.end()) ? it->second : 0.0f;
    };

    // Total arc: flexion + extension
    results["total_arc_left"] = total_arc(get("left_forward_flexion"), get("left_extension"));
    results["total_arc_right"] = total_arc(get("right_forward_flexion"), get("right_extension"));

    // Bilateral comparison: affected vs unaffected
    results["bilateral_flexion"] = bilateral_comparison(get("left_forward_flexion"), get("right_forward_flexion"));
    results["bilateral_abduction"] = bilateral_comparison(get("left_abduction"), get("right_abduction"));

    // Symmetry index
    results["symmetry_flexion"] = symmetry_index(get("left_forward_flexion"), get("right_forward_flexion"));
    results["symmetry_abduction"] = symmetry_index(get("left_abduction"), get("right_abduction"));

    // ROM percentage (against normal 180° for flexion/abduction)
    results["rom_pct_left_flexion"] = rom_percentage(get("left_forward_flexion"), 180.0f);
    results["rom_pct_right_flexion"] = rom_percentage(get("right_forward_flexion"), 180.0f);
    results["rom_pct_left_abduction"] = rom_percentage(get("left_abduction"), 180.0f);
    results["rom_pct_right_abduction"] = rom_percentage(get("right_abduction"), 180.0f);

    return results;
}

} // namespace oculus