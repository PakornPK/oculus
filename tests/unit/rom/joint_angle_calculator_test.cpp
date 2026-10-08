#include <gtest/gtest.h>
#include "oculus/rom/joint_angle_calculator.hpp"

using namespace oculus;

TEST(JointAngleCalculatorTest, AngleFromVerticalDown) {
    Keypoint shoulder = {200, 200, 0.9f};
    Keypoint elbow = {200, 300, 0.9f};
    Keypoint hip = {200, 400, 0.9f};

    float angle = JointAngleCalculator::angle_from_vertical(shoulder, elbow, hip);
    EXPECT_NEAR(angle, 0.0f, 5.0f);
}

TEST(JointAngleCalculatorTest, AngleFromVertical90) {
    Keypoint shoulder = {200, 200, 0.9f};
    Keypoint elbow = {300, 200, 0.9f};
    Keypoint hip = {200, 400, 0.9f};

    float angle = JointAngleCalculator::angle_from_vertical(shoulder, elbow, hip);
    EXPECT_NEAR(angle, 90.0f, 5.0f);
}

TEST(JointAngleCalculatorTest, VerticalDistance) {
    Keypoint a = {100, 200, 0.9f};
    Keypoint b = {100, 300, 0.9f};

    float dist = JointAngleCalculator::vertical_distance(a, b);
    EXPECT_FLOAT_EQ(dist, 100.0f);
}

TEST(JointAngleCalculatorTest, HorizontalOffset) {
    Keypoint a = {250, 200, 0.9f};
    Keypoint b = {200, 200, 0.9f};

    float offset = JointAngleCalculator::horizontal_offset(a, b);
    EXPECT_FLOAT_EQ(offset, 50.0f);
}

TEST(JointAngleCalculatorTest, CalculateAngleAbduction) {
    Pose pose;
    pose.keypoints.resize(17);

    pose.keypoints[5] = {200, 200, 0.9f};
    pose.keypoints[7] = {300, 200, 0.9f};
    pose.keypoints[11] = {200, 400, 0.9f};

    JointAngleCalculator calc;
    float angle = calc.calculate_angle(pose, MovementType::ABDUCTION, Side::LEFT);
    EXPECT_GT(angle, 0.0f);
}