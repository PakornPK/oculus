#include <gtest/gtest.h>
#include "oculus/rom/roi.hpp"

using namespace oculus;

TEST(ROIManagerTest, CircleContains) {
    ROIRegion r;
    r.shape = ROIShape::CIRCLE;
    r.center = {100, 100};
    r.width = 80.0f;

    EXPECT_TRUE(r.contains({100, 100}));
    EXPECT_TRUE(r.contains({120, 100}));
    EXPECT_FALSE(r.contains({200, 200}));
}

TEST(ROIManagerTest, EllipseContains) {
    ROIRegion r;
    r.shape = ROIShape::ELLIPSE;
    r.center = {100, 100};
    r.width = 100.0f;
    r.height = 60.0f;

    EXPECT_TRUE(r.contains({100, 100}));
    EXPECT_TRUE(r.contains({140, 100}));
    EXPECT_FALSE(r.contains({200, 200}));
}

TEST(ROIManagerTest, DefaultROIs) {
    auto ls = ROIManager::left_shoulder_roi();
    EXPECT_EQ(ls.name, "Left Shoulder");
    EXPECT_EQ(ls.shape, ROIShape::CIRCLE);
}