#include "oculus/rom/roi.hpp"
#include <cmath>

namespace oculus {

bool ROIRegion::contains(const Point2D& point) const {
    float dx = point.x - center.x;
    float dy = point.y - center.y;

    // Apply inverse rotation
    float rad = -rotation_deg * M_PI / 180.0f;
    float cos_r = std::cos(rad);
    float sin_r = std::sin(rad);
    float rx = dx * cos_r - dy * sin_r;
    float ry = dx * sin_r + dy * cos_r;

    switch (shape) {
        case ROIShape::CIRCLE: {
            float dist = std::sqrt(rx * rx + ry * ry);
            return dist <= width / 2.0f && dist >= inner_radius;
        }
        case ROIShape::ELLIPSE: {
            float a = width / 2.0f;
            float b = height / 2.0f;
            if (a <= 0 || b <= 0) return false;
            float val = (rx * rx) / (a * a) + (ry * ry) / (b * b);
            return val <= 1.0f;
        }
        case ROIShape::RECTANGLE:
            return std::abs(rx) <= width / 2.0f && std::abs(ry) <= height / 2.0f;
        default:
            return false;
    }
}

float ROIRegion::area() const {
    switch (shape) {
        case ROIShape::CIRCLE:
            return M_PI * (width / 2.0f) * (width / 2.0f);
        case ROIShape::ELLIPSE:
            return M_PI * (width / 2.0f) * (height / 2.0f);
        case ROIShape::RECTANGLE:
            return width * height;
        default:
            return 0.0f;
    }
}

void ROIManager::add_region(const ROIRegion& region) {
    regions_.push_back(region);
}

void ROIManager::remove_region(const std::string& id) {
    regions_.erase(
        std::remove_if(regions_.begin(), regions_.end(),
            [&](const ROIRegion& r) { return r.id == id; }),
        regions_.end());
}

ROIRegion* ROIManager::get_region(const std::string& id) {
    for (auto& r : regions_) {
        if (r.id == id) return &r;
    }
    return nullptr;
}

std::vector<ROIRegion> ROIManager::get_all_regions() const {
    return regions_;
}

ROIRegion ROIManager::left_shoulder_roi() {
    ROIRegion r;
    r.id = "left_shoulder";
    r.name = "Left Shoulder";
    r.shape = ROIShape::CIRCLE;
    r.center = {280, 200};
    r.width = 80.0f;
    return r;
}

ROIRegion ROIManager::right_shoulder_roi() {
    ROIRegion r;
    r.id = "right_shoulder";
    r.name = "Right Shoulder";
    r.shape = ROIShape::CIRCLE;
    r.center = {360, 200};
    r.width = 80.0f;
    return r;
}

ROIRegion ROIManager::left_elbow_roi() {
    ROIRegion r;
    r.id = "left_elbow";
    r.name = "Left Elbow";
    r.shape = ROIShape::CIRCLE;
    r.center = {260, 300};
    r.width = 60.0f;
    return r;
}

ROIRegion ROIManager::right_elbow_roi() {
    ROIRegion r;
    r.id = "right_elbow";
    r.name = "Right Elbow";
    r.shape = ROIShape::CIRCLE;
    r.center = {380, 300};
    r.width = 60.0f;
    return r;
}

} // namespace oculus