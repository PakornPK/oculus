#include <gtest/gtest.h>
#include "oculus/rom/shoulder_rom_analyzer.hpp"

using namespace oculus;

TEST(ShoulderRomAnalyzerTest, TracksMinMaxROM) {
    ShoulderRomAnalyzer analyzer;

    Pose pose1;
    pose1.keypoints.resize(17);
    pose1.keypoints[5] = {200, 200, 0.9f};
    pose1.keypoints[7] = {200, 300, 0.9f};
    pose1.keypoints[11] = {200, 400, 0.9f};
    pose1.keypoints[6] = {300, 200, 0.9f};
    pose1.keypoints[8] = {300, 300, 0.9f};
    pose1.keypoints[12] = {300, 400, 0.9f};
    pose1.keypoints[9] = {200, 350, 0.9f};
    pose1.keypoints[10] = {300, 350, 0.9f};
    pose1.keypoints[3] = {180, 180, 0.9f};
    pose1.keypoints[4] = {320, 180, 0.9f};

    analyzer.update(pose1);

    auto rom = analyzer.get_rom(MovementType::ABDUCTION, Side::LEFT);
    EXPECT_GE(rom.sample_count, 1);
}

TEST(ShoulderRomAnalyzerTest, ResetClearsData) {
    ShoulderRomAnalyzer analyzer;

    Pose pose;
    pose.keypoints.resize(17);
    analyzer.update(pose);

    analyzer.reset();
    auto rom = analyzer.get_rom(MovementType::ABDUCTION, Side::LEFT);
    EXPECT_EQ(rom.sample_count, 0);
}

TEST(ShoulderRomAnalyzerTest, MovementNames) {
    EXPECT_EQ(ShoulderRomAnalyzer::movement_name(MovementType::ABDUCTION), "abduction");
    EXPECT_EQ(ShoulderRomAnalyzer::movement_name(MovementType::FORWARD_FLEXION), "forward_flexion");
    EXPECT_EQ(ShoulderRomAnalyzer::movement_name(MovementType::SHOULDER_ELEVATION), "shoulder_elevation");
}

TEST(ShoulderRomAnalyzerTest, ResultHasBothSides) {
    ShoulderRomAnalyzer analyzer;

    Pose pose;
    pose.keypoints.resize(17);
    pose.keypoints[5] = {200, 200, 0.9f};
    pose.keypoints[7] = {250, 250, 0.9f};
    pose.keypoints[11] = {200, 400, 0.9f};
    pose.keypoints[6] = {300, 200, 0.9f};
    pose.keypoints[8] = {350, 250, 0.9f};
    pose.keypoints[12] = {300, 400, 0.9f};
    pose.keypoints[9] = {260, 300, 0.9f};
    pose.keypoints[10] = {360, 300, 0.9f};
    pose.keypoints[3] = {180, 180, 0.9f};
    pose.keypoints[4] = {320, 180, 0.9f};

    analyzer.update(pose);
    auto result = analyzer.get_result();

    EXPECT_FALSE(result.left.empty());
    EXPECT_FALSE(result.right.empty());
}