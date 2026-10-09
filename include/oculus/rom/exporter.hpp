#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace oculus {

struct ExportData {
    std::string session_id;
    std::string patient_id;
    std::string date;
    std::string side;
    std::string movement;
    float angle_a = 0.0f;
    float angle_b = 0.0f;
    float rom = 0.0f;
    std::string classification;
};

class DataExporter {
public:
    static bool export_csv(const std::string& path, const std::vector<ExportData>& data);
    static bool export_json(const std::string& path, const std::vector<ExportData>& data);
    static std::string generate_report(const std::vector<ExportData>& data);
};

} // namespace oculus