#include <gtest/gtest.h>
#include "oculus/rom/pipeline.hpp"

using namespace oculus;

TEST(MeasurementPipelineTest, StartSession) {
    MeasurementPipeline pipeline;
    pipeline.start_session("patient-001");
    EXPECT_EQ(pipeline.get_stage(), MeasurementStage::SCAN);
}

TEST(MeasurementPipelineTest, AddMeasurement) {
    MeasurementPipeline pipeline;
    pipeline.add_measurement("forward_flexion", "left", 25.0f, 155.0f);

    auto result = pipeline.get_result("forward_flexion", "left");
    EXPECT_FLOAT_EQ(result.rom, 130.0f);
    // 130° ROM = Severe (not Moderate)
    EXPECT_EQ(result.classification, "Severe");
}

TEST(MeasurementPipelineTest, Classification) {
    MeasurementPipeline pipeline;
    pipeline.add_measurement("test", "left", 0.0f, 5.0f);   // Normal
    pipeline.add_measurement("test2", "left", 0.0f, 20.0f);  // Mild
    pipeline.add_measurement("test3", "left", 0.0f, 50.0f);  // Moderate
    pipeline.add_measurement("test4", "left", 0.0f, 100.0f); // Severe

    EXPECT_EQ(pipeline.get_result("test", "left").classification, "Normal");
    EXPECT_EQ(pipeline.get_result("test2", "left").classification, "Mild");
    EXPECT_EQ(pipeline.get_result("test3", "left").classification, "Moderate");
    EXPECT_EQ(pipeline.get_result("test4", "left").classification, "Severe");
}

TEST(MeasurementPipelineTest, GenerateReport) {
    MeasurementPipeline pipeline;
    pipeline.start_session("patient-001");
    pipeline.add_measurement("forward_flexion", "left", 25.0f, 155.0f);

    auto report = pipeline.generate_report();
    EXPECT_TRUE(report.find("Shoulder ROM Assessment") != std::string::npos);
    EXPECT_TRUE(report.find("forward_flexion") != std::string::npos);
}

TEST(MeasurementPipelineTest, MarkersLoaded) {
    MeasurementPipeline pipeline;
    auto markers = pipeline.markers().get_all_markers();
    EXPECT_GE(markers.size(), 14u);
}

TEST(MeasurementPipelineTest, ROIsLoaded) {
    MeasurementPipeline pipeline;
    auto rois = pipeline.rois().get_all_regions();
    EXPECT_GE(rois.size(), 4u);
}