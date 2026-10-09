#pragma once

#include <cmath>
#include <string>

namespace oculus {

struct Point2D {
    float x = 0.0f;
    float y = 0.0f;
    float confidence = 0.0f;

    Point2D() = default;
    Point2D(float x, float y, float conf = 1.0f) : x(x), y(y), confidence(conf) {}

    float distance_to(const Point2D& other) const {
        float dx = x - other.x;
        float dy = y - other.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    Point2D midpoint(const Point2D& other) const {
        return {(x + other.x) / 2.0f, (y + other.y) / 2.0f, std::min(confidence, other.confidence)};
    }
};

struct Line2D {
    Point2D start;
    Point2D end;

    float length() const { return start.distance_to(end); }

    float angle_degrees() const {
        float dx = end.x - start.x;
        float dy = end.y - start.y;
        return std::atan2(dy, dx) * 180.0f / M_PI;
    }

    Point2D midpoint() const { return start.midpoint(end); }
};

struct Angle2D {
    Line2D line_a;
    Line2D line_b;

    float measure() const {
        float angle_a = line_a.angle_degrees();
        float angle_b = line_b.angle_degrees();
        float diff = std::abs(angle_a - angle_b);
        if (diff > 180.0f) diff = 360.0f - diff;
        return diff;
    }
};

struct Circle2D {
    Point2D center;
    float radius = 0.0f;

    bool contains(const Point2D& point) const {
        return center.distance_to(point) <= radius;
    }

    float circumference() const {
        return 2.0f * M_PI * radius;
    }

    float area() const {
        return M_PI * radius * radius;
    }
};

} // namespace oculus