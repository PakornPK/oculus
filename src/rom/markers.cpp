#include "oculus/rom/markers.hpp"
#include <algorithm>

namespace oculus {

std::string MeasurementMarker::classify() const {
    if (measured_value >= threshold_normal_min && measured_value <= threshold_normal_max) {
        return "Normal";
    }
    return "Abnormal";
}

void MarkerManager::add_marker(const MeasurementMarker& marker) {
    markers_[marker.id] = marker;
}

void MarkerManager::remove_marker(const std::string& id) {
    markers_.erase(id);
}

MeasurementMarker* MarkerManager::get_marker(const std::string& id) {
    auto it = markers_.find(id);
    return (it != markers_.end()) ? &it->second : nullptr;
}

std::vector<MeasurementMarker> MarkerManager::get_markers_by_side(const std::string& side) const {
    std::vector<MeasurementMarker> result;
    for (const auto& [id, marker] : markers_) {
        if (marker.side == side || marker.side == "both") {
            result.push_back(marker);
        }
    }
    return result;
}

std::vector<MeasurementMarker> MarkerManager::get_all_markers() const {
    std::vector<MeasurementMarker> result;
    for (const auto& [id, marker] : markers_) {
        result.push_back(marker);
    }
    return result;
}

void MarkerManager::update_measurement(const std::string& id, float value) {
    auto* marker = get_marker(id);
    if (marker) {
        marker->measured_value = value;
        marker->classification = marker->classify();
    }
}

void MarkerManager::clear() {
    markers_.clear();
}

std::vector<MeasurementMarker> MarkerManager::default_shoulder_markers() {
    std::vector<MeasurementMarker> markers;

    auto add = [&](const std::string& id, const std::string& name, const std::string& side,
                   float normal_min, float normal_max) {
        MeasurementMarker m;
        m.id = id;
        m.name = name;
        m.type = MarkerType::ANGLE;
        m.side = side;
        m.threshold_normal_min = normal_min;
        m.threshold_normal_max = normal_max;
        markers.push_back(m);
    };

    // Left shoulder
    add("l_flexion", "Left Forward Flexion", "left", 150.0f, 180.0f);
    add("l_abduction", "Left Abduction", "left", 150.0f, 180.0f);
    add("l_extension", "Left Extension", "left", 40.0f, 60.0f);
    add("l_ext_rotation", "Left External Rotation", "left", 80.0f, 90.0f);
    add("l_int_rotation", "Left Internal Rotation", "left", 70.0f, 80.0f);
    add("l_adduction", "Left Adduction", "left", 30.0f, 40.0f);
    add("l_horz_adduction", "Left Horizontal Adduction", "left", 120.0f, 130.0f);

    // Right shoulder
    add("r_flexion", "Right Forward Flexion", "right", 150.0f, 180.0f);
    add("r_abduction", "Right Abduction", "right", 150.0f, 180.0f);
    add("r_extension", "Right Extension", "right", 40.0f, 60.0f);
    add("r_ext_rotation", "Right External Rotation", "right", 80.0f, 90.0f);
    add("r_int_rotation", "Right Internal Rotation", "right", 70.0f, 80.0f);
    add("r_adduction", "Right Adduction", "right", 30.0f, 40.0f);
    add("r_horz_adduction", "Right Horizontal Adduction", "right", 120.0f, 130.0f);

    return markers;
}

} // namespace oculus