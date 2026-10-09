#include "oculus/rom/exporter.hpp"
#include <fstream>
#include <sstream>

namespace oculus {

bool DataExporter::export_csv(const std::string& path, const std::vector<ExportData>& data) {
    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "session_id,patient_id,date,side,movement,angle_a,angle_b,rom,classification\n";
    for (const auto& d : data) {
        file << d.session_id << ","
             << d.patient_id << ","
             << d.date << ","
             << d.side << ","
             << d.movement << ","
             << d.angle_a << ","
             << d.angle_b << ","
             << d.rom << ","
             << d.classification << "\n";
    }
    return true;
}

bool DataExporter::export_json(const std::string& path, const std::vector<ExportData>& data) {
    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "{\n  \"measurements\": [\n";
    for (size_t i = 0; i < data.size(); ++i) {
        const auto& d = data[i];
        file << "    {\n";
        file << "      \"session\": \"" << d.session_id << "\",\n";
        file << "      \"side\": \"" << d.side << "\",\n";
        file << "      \"movement\": \"" << d.movement << "\",\n";
        file << "      \"angle_a\": " << d.angle_a << ",\n";
        file << "      \"angle_b\": " << d.angle_b << ",\n";
        file << "      \"rom\": " << d.rom << ",\n";
        file << "      \"classification\": \"" << d.classification << "\"\n";
        file << "    }" << (i < data.size() - 1 ? "," : "") << "\n";
    }
    file << "  ]\n}\n";
    return true;
}

std::string DataExporter::generate_report(const std::vector<ExportData>& data) {
    std::ostringstream report;
    report << "=== Shoulder ROM Assessment Report ===\n\n";
    report << "Session: " << (data.empty() ? "N/A" : data[0].session_id) << "\n";
    report << "Date: " << (data.empty() ? "N/A" : data[0].date) << "\n\n";

    report << "Movement                    Side    A(°)    B(°)    ROM(°)  Status\n";
    report << "------------------------    ----    ----    ----    ------  ------\n";

    for (const auto& d : data) {
        char line[128];
        snprintf(line, sizeof(line), "%-28s %-6s %6.1f %6.1f %6.1f  %s\n",
                 d.movement.c_str(), d.side.c_str(),
                 d.angle_a, d.angle_b, d.rom, d.classification.c_str());
        report << line;
    }
    return report.str();
}

} // namespace oculus