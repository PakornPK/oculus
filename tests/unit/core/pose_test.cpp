#include <gtest/gtest.h>
#include "oculus/core/pose.hpp"

using namespace oculus;

TEST(PoseTest, KeypointDefaultValues) {
    Keypoint kp;
    EXPECT_FLOAT_EQ(kp.x, 0.0f);
    EXPECT_FLOAT_EQ(kp.y, 0.0f);
    EXPECT_FLOAT_EQ(kp.confidence, 0.0f);
}

TEST(PoseTest, PoseHasKeypoints) {
    Pose pose;
    pose.keypoints.resize(KEYPOINT_COUNT);
    pose.confidence = 0.95f;

    EXPECT_EQ(pose.keypoints.size(), KEYPOINT_COUNT);
    EXPECT_FLOAT_EQ(pose.confidence, 0.95f);
}

TEST(PoseTest, PoseResultHoldsMultiplePoses) {
    PoseResult result;
    result.timestamp = 12345;

    Pose pose1;
    pose1.keypoints.resize(KEYPOINT_COUNT);
    pose1.confidence = 0.9f;

    Pose pose2;
    pose2.keypoints.resize(KEYPOINT_COUNT);
    pose2.confidence = 0.8f;

    result.poses.push_back(pose1);
    result.poses.push_back(pose2);

    EXPECT_EQ(result.poses.size(), 2u);
    EXPECT_EQ(result.timestamp, 12345);
}

TEST(PoseTest, KeypointIndexValues) {
    EXPECT_EQ(static_cast<int>(KeypointIndex::LEFT_SHOULDER), 5);
    EXPECT_EQ(static_cast<int>(KeypointIndex::RIGHT_SHOULDER), 6);
    EXPECT_EQ(static_cast<int>(KeypointIndex::LEFT_ELBOW), 7);
    EXPECT_EQ(static_cast<int>(KeypointIndex::RIGHT_ELBOW), 8);
    EXPECT_EQ(static_cast<int>(KeypointIndex::LEFT_WRIST), 9);
    EXPECT_EQ(static_cast<int>(KeypointIndex::RIGHT_WRIST), 10);
}