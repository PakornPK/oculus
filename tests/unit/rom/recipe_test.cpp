#include <gtest/gtest.h>
#include "oculus/rom/recipe.hpp"

using namespace oculus;

TEST(RecipeTest, DefaultShoulderROM) {
    auto recipe = Recipe::default_shoulder_rom();
    EXPECT_EQ(recipe.name, "Standard Shoulder ROM");
    EXPECT_EQ(recipe.side, "both");
    EXPECT_EQ(recipe.reps_per_movement, 3);
    EXPECT_GE(recipe.movements.size(), 7u);
}

TEST(RecipeTest, ThresholdClassification) {
    Threshold t;
    t.normal_min = 150.0f;
    t.normal_max = 180.0f;
    t.mild_min = 120.0f;
    t.mild_max = 150.0f;
    t.moderate_min = 90.0f;
    t.moderate_max = 120.0f;

    EXPECT_EQ(t.classify(160.0f), "Normal");
    EXPECT_EQ(t.classify(130.0f), "Mild");
    EXPECT_EQ(t.classify(100.0f), "Moderate");
    EXPECT_EQ(t.classify(50.0f), "Severe");
}

TEST(RecipeTest, MovementConfigs) {
    auto recipe = Recipe::default_shoulder_rom();
    EXPECT_EQ(recipe.movements[0].name, "forward_flexion");
    EXPECT_EQ(recipe.movements[0].threshold.normal_min, 150.0f);
}

TEST(RecipeTest, AnchoringConfig) {
    auto recipe = Recipe::default_shoulder_rom();
    EXPECT_EQ(recipe.anchoring.reference_landmark, "shoulder_center");
    EXPECT_TRUE(recipe.anchoring.compensate_rotation);
}