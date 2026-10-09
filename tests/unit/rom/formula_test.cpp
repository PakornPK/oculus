#include <gtest/gtest.h>
#include "oculus/rom/formula.hpp"

using namespace oculus;

TEST(MeasurementFormulaTest, TotalArc) {
    EXPECT_FLOAT_EQ(MeasurementFormula::total_arc(150.0f, 40.0f), 190.0f);
    EXPECT_FLOAT_EQ(MeasurementFormula::total_arc(0.0f, 0.0f), 0.0f);
}

TEST(MeasurementFormulaTest, BilateralComparison) {
    EXPECT_FLOAT_EQ(MeasurementFormula::bilateral_comparison(150.0f, 160.0f), 93.75f);
    EXPECT_FLOAT_EQ(MeasurementFormula::bilateral_comparison(0.0f, 0.0f), 0.0f);
}

TEST(MeasurementFormulaTest, ROMPercentage) {
    EXPECT_NEAR(MeasurementFormula::rom_percentage(160.0f, 180.0f), 88.89f, 0.1f);
    EXPECT_FLOAT_EQ(MeasurementFormula::rom_percentage(0.0f, 180.0f), 0.0f);
}

TEST(MeasurementFormulaTest, SymmetryIndex) {
    EXPECT_FLOAT_EQ(MeasurementFormula::symmetry_index(150.0f, 160.0f), 93.75f);
    EXPECT_FLOAT_EQ(MeasurementFormula::symmetry_index(160.0f, 160.0f), 100.0f);
}

TEST(MeasurementFormulaTest, MovementVelocity) {
    EXPECT_FLOAT_EQ(MeasurementFormula::movement_velocity(0.0f, 90.0f, 2.0f), 45.0f);
    EXPECT_FLOAT_EQ(MeasurementFormula::movement_velocity(90.0f, 0.0f, 2.0f), 45.0f);
}

TEST(MeasurementFormulaTest, ComputeAll) {
    MeasurementMap raw;
    raw["left_forward_flexion"] = 150.0f;
    raw["right_forward_flexion"] = 160.0f;
    raw["left_extension"] = 40.0f;
    raw["right_extension"] = 45.0f;
    raw["left_abduction"] = 140.0f;
    raw["right_abduction"] = 155.0f;

    auto results = MeasurementFormula::compute_all(raw);

    EXPECT_FLOAT_EQ(results["total_arc_left"], 190.0f);
    EXPECT_FLOAT_EQ(results["total_arc_right"], 205.0f);
    EXPECT_GT(results["symmetry_flexion"], 90.0f);
    EXPECT_GT(results["rom_pct_left_flexion"], 80.0f);
}