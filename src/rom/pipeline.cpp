#include "oculus/rom/pipeline.hpp"
#include <spdlog/spdlog.h>
#include <sstream>

namespace oculus {

MeasurementPipeline::MeasurementPipeline()
    : stage_(MeasurementStage::SCAN) {
    marker_mgr_ = MarkerManager();
    auto markers = MarkerManager::default_shoulder_markers();
    for (const auto& m : markers) {
        marker_mgr_.add_marker(m);
    }
    roi_mgr_.add_region(ROIManager::left_shoulder_roi());
    roi_mgr_.add_region(ROIManager::right_shoulder_roi());
    roi_mgr_.add_region(ROIManager::left_elbow_roi());
    roi_mgr_.add_region(ROIManager::right_elbow_roi());
}

void MeasurementPipeline::start_session(const std::string& patient_id) {
    patient_id_ = patient_id;
    session_id_ = "session_" + patient_id + "_" + std::to_string(results_.size());
    stage_ = MeasurementStage::SCAN;
    results_.clear();
    spdlog::info("Measurement session started: {}", session_id_);
}

void MeasurementPipeline::capture_frame(const std::string& phase) {
    spdlog::info("Frame captured (phase={})", phase);
}

void MeasurementPipeline::add_measurement(
    const std::string& movement, const std::string& side,
    float angle_a, float angle_b) {

    MeasurementResult result;
    result.movement = movement;
    result.side = side;
    result.angle_a = angle_a;
    result.angle_b = angle_b;
    result.rom = angle_b - angle_a;

    // Classify
    float abs_rom = std::abs(result.rom);
    if (abs_rom < 10.0f) result.classification = "Normal";
    else if (abs_rom < 30.0f) result.classification = "Mild";
    else if (abs_rom < 60.0f) result.classification = "Moderate";
    else result.classification = "Severe";

    results_.push_back(result);
    spdlog::info("Measurement: {} {} = {:.1f}° ROM ({})", movement, side, result.rom, result.classification);
}

MeasurementResult MeasurementPipeline::get_result(
    const std::string& movement, const std::string& side) const {
    for (const auto& r : results_) {
        if (r.movement == movement && r.side == side) return r;
    }
    return {};
}

std::vector<MeasurementResult> MeasurementPipeline::get_all_results() const {
    return results_;
}

std::string MeasurementPipeline::generate_report() const {
    std::ostringstream report;
    report << "=== Shoulder ROM Assessment Report ===\n\n";
    report << "Patient: " << patient_id_ << "\n";
    report << "Session: " << session_id_ << "\n\n";
    report << "Movement                    Side    A(°)    B(°)    ROM(°)  Status\n";
    report << "------------------------    ----    ----    ----    ------  ------\n";

    for (const auto& r : results_) {
        char line[128];
        snprintf(line, sizeof(line), "%-28s %-6s %6.1f %6.1f %6.1f  %s\n",
                 r.movement.c_str(), r.side.c_str(),
                 r.angle_a, r.angle_b, r.rom, r.classification.c_str());
        report << line;
    }
    return report.str();
}

void MeasurementPipeline::set_stage(MeasurementStage stage) {
    stage_ = stage;
}

MeasurementStage MeasurementPipeline::get_stage() const {
    return stage_;
}

} // namespace oculus