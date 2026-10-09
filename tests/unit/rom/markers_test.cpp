#include <gtest/gtest.h>
#include "oculus/rom/markers.hpp"

using namespace oculus;

TEST(MarkerManagerTest, AddAndGetMarker) {
    MarkerManager mgr;
    MeasurementMarker m;
    m.id = "test";
    m.name = "Test Marker";
    m.type = MarkerType::ANGLE;
    m.side = "left";
    mgr.add_marker(m);

    auto* result = mgr.get_marker("test");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->name, "Test Marker");
}

TEST(MarkerManagerTest, RemoveMarker) {
    MarkerManager mgr;
    MeasurementMarker m;
    m.id = "test";
    mgr.add_marker(m);
    mgr.remove_marker("test");
    EXPECT_EQ(mgr.get_marker("test"), nullptr);
}

TEST(MarkerManagerTest, GetBySide) {
    MarkerManager mgr;
    MeasurementMarker ml, mr;
    ml.id = "l"; ml.side = "left";
    mr.id = "r"; mr.side = "right";
    mgr.add_marker(ml);
    mgr.add_marker(mr);

    auto left = mgr.get_markers_by_side("left");
    EXPECT_EQ(left.size(), 1u);
    EXPECT_EQ(left[0].id, "l");
}

TEST(MarkerManagerTest, DefaultShoulderMarkers) {
    auto markers = MarkerManager::default_shoulder_markers();
    EXPECT_GE(markers.size(), 14u);
    EXPECT_EQ(markers[0].name, "Left Forward Flexion");
}

TEST(MarkerManagerTest, UpdateMeasurement) {
    MarkerManager mgr;
    MeasurementMarker m;
    m.id = "test";
    m.threshold_normal_min = 150.0f;
    m.threshold_normal_max = 180.0f;
    mgr.add_marker(m);

    mgr.update_measurement("test", 160.0f);
    auto* result = mgr.get_marker("test");
    EXPECT_FLOAT_EQ(result->measured_value, 160.0f);
    EXPECT_EQ(result->classification, "Normal");
}