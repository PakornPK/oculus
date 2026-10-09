#include <gtest/gtest.h>
#include "oculus/core/geometry.hpp"

using namespace oculus;

TEST(GeometryTest, PointDistance) {
    Point2D a(0, 0), b(3, 4);
    EXPECT_FLOAT_EQ(a.distance_to(b), 5.0f);
}

TEST(GeometryTest, PointMidpoint) {
    Point2D a(0, 0), b(10, 10);
    auto mid = a.midpoint(b);
    EXPECT_FLOAT_EQ(mid.x, 5.0f);
    EXPECT_FLOAT_EQ(mid.y, 5.0f);
}

TEST(GeometryTest, LineLength) {
    Line2D line{{0, 0}, {3, 4}};
    EXPECT_FLOAT_EQ(line.length(), 5.0f);
}

TEST(GeometryTest, LineAngle) {
    Line2D horizontal{{0, 0}, {10, 0}};
    EXPECT_NEAR(horizontal.angle_degrees(), 0.0f, 0.1f);

    Line2D vertical{{0, 0}, {0, 10}};
    EXPECT_NEAR(vertical.angle_degrees(), 90.0f, 0.1f);
}

TEST(GeometryTest, AngleBetweenLines) {
    Line2D a{{0, 0}, {10, 0}};
    Line2D b{{0, 0}, {0, 10}};
    Angle2D angle{a, b};
    EXPECT_NEAR(angle.measure(), 90.0f, 0.1f);
}

TEST(GeometryTest, CircleContains) {
    Circle2D circle{{100, 100}, 50};
    EXPECT_TRUE(circle.contains({100, 100}));
    EXPECT_TRUE(circle.contains({120, 100}));
    EXPECT_FALSE(circle.contains({200, 200}));
}

TEST(GeometryTest, CircleArea) {
    Circle2D circle{{0, 0}, 10};
    EXPECT_NEAR(circle.area(), 314.16f, 0.1f);
}