#include <gtest/gtest.h>
#include "oculus/rom/exporter.hpp"
#include <cstdio>
#include <fstream>

using namespace oculus;

TEST(DataExporterTest, ExportCSV) {
    std::vector<ExportData> data = {
        {"s1", "p1", "2026-01-01", "left", "flexion", 25.0f, 155.0f, 130.0f, "Moderate"},
        {"s1", "p1", "2026-01-01", "right", "flexion", 20.0f, 160.0f, 140.0f, "Mild"},
    };

    EXPECT_TRUE(DataExporter::export_csv("/tmp/test_export.csv", data));

    std::ifstream file("/tmp/test_export.csv");
    std::string line;
    std::getline(file, line);
    EXPECT_TRUE(line.find("session_id") != std::string::npos);
    std::getline(file, line);
    EXPECT_TRUE(line.find("flexion") != std::string::npos);
}

TEST(DataExporterTest, ExportJSON) {
    std::vector<ExportData> data = {
        {"s1", "p1", "2026-01-01", "left", "abduction", 10.0f, 170.0f, 160.0f, "Normal"},
    };

    EXPECT_TRUE(DataExporter::export_json("/tmp/test_export.json", data));
}

TEST(DataExporterTest, GenerateReport) {
    std::vector<ExportData> data = {
        {"s1", "p1", "2026-01-01", "left", "forward_flexion", 25.0f, 155.0f, 130.0f, "Moderate"},
    };

    auto report = DataExporter::generate_report(data);
    EXPECT_TRUE(report.find("Shoulder ROM Assessment") != std::string::npos);
    EXPECT_TRUE(report.find("forward_flexion") != std::string::npos);
}