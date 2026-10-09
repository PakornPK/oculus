#pragma once

#include "oculus/core/geometry.hpp"
#include <string>
#include <vector>

namespace oculus {

enum class ROIShape {
    CIRCLE,
    ELLIPSE,
    RECTANGLE,
    POLYGON
};

struct ROIRegion {
    std::string id;
    std::string name;
    ROIShape shape;
    Point2D center;
    float width = 0.0f;
    float height = 0.0f;
    float rotation_deg = 0.0f;
    float inner_radius = 0.0f;  // For annular regions

    // For polygon
    std::vector<Point2D> vertices;

    bool contains(const Point2D& point) const;
    float area() const;
};

class ROIManager {
public:
    void add_region(const ROIRegion& region);
    void remove_region(const std::string& id);
    ROIRegion* get_region(const std::string& id);
    std::vector<ROIRegion> get_all_regions() const;

    // Predefined ROIs for shoulder
    static ROIRegion left_shoulder_roi();
    static ROIRegion right_shoulder_roi();
    static ROIRegion left_elbow_roi();
    static ROIRegion right_elbow_roi();

private:
    std::vector<ROIRegion> regions_;
};

} // namespace oculus