#include <gtest/gtest.h>
#include "oculus/rom/calibrator.hpp"

using namespace oculus;

TEST(CalibratorTest, AutoCalibrate) {
    Calibrator cal;
    Pose pose;
    pose.keypoints.resize(17);
    pose.keypoints[5] = {200, 200, 0.9f};
    pose.keypoints[6] = {300, 200, 0.9f};

    auto data = cal.auto_calibrate(pose);
    EXPECT_TRUE(data.calibrated);
    EXPECT_GT(data.shoulder_width_pixels, 0.0f);
    EXPECT_GT(data.pixels_per_meter, 0.0f);
}

TEST(CalibratorTest, StandardCalibrate) {
    Calibrator cal;
    Pose pose;
    pose.keypoints.resize(17);
    pose.keypoints[5] = {200, 200, 0.9f};
    pose.keypoints[6] = {300, 200, 0.9f};

    auto data = cal.standard_calibrate(pose, 1.75f);
    EXPECT_TRUE(data.calibrated);
    EXPECT_FLOAT_EQ(data.subject_height_m, 1.75f);
    EXPECT_EQ(data.level, CalibrationLevel::STANDARD);
}

TEST(CalibratorTest, ResetClearsData) {
    Calibrator cal;
    Pose pose;
    pose.keypoints.resize(17);
    pose.keypoints[5] = {200, 200, 0.9f};
    pose.keypoints[6] = {300, 200, 0.9f};
    cal.auto_calibrate(pose);

    cal.reset();
    EXPECT_FALSE(cal.is_calibrated());
}