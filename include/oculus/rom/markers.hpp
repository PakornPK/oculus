#pragma once

#include "oculus/core/geometry.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace oculus {

enum class MarkerType {
    POINT,      // Single point on body
    LINE,       // Line between two points
    ANGLE,      // Angle between two lines
    CIRCLE,     // Circular ROI around joint
    ELLIPSE     // Elliptical ROI along limb
};

struct MeasurementMarker {
    std::string id;
    std::string name;
    MarkerType type;
    std::string side;  // "left", "right", "both"

    // Geometry
    Point2D point;
    Line2D line;
    Angle2D angle;
    Circle2D circle;

    // ROI
    float roi_radius = 0.0f;
    float roi_rotation = 0.0f;

    // Thresholds
    float threshold_normal_min = 0.0f;
    float threshold_normal_max = 180.0f;

    // Result
    float measured_value = 0.0f;
    std::string classification;

    std::string classify() const;
};

class MarkerManager {
public:
    void add_marker(const MeasurementMarker& marker);
    void remove_marker(const std::string& id);
    MeasurementMarker* get_marker(const std::string& id);
    std::vector<MeasurementMarker> get_markers_by_side(const std::string& side) const;
    std::vector<MeasurementMarker> get_all_markers() const;

    void update_measurement(const std::string& id, float value);
    void clear();

    // Predefined markers for shoulder ROM
    static std::vector<MeasurementMarker> default_shoulder_markers();

private:
    std::unordered_map<std::string, MeasurementMarker> markers_;
};

} // namespace oculus