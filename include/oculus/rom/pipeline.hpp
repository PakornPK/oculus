#pragma once

#include "oculus/rom/markers.hpp"
#include "oculus/rom/roi.hpp"
#include "oculus/rom/formula.hpp"
#include "oculus/rom/exporter.hpp"
#include "oculus/core/geometry.hpp"
#include <string>
#include <vector>
#include <functional>

namespace oculus {

enum class MeasurementStage {
    SCAN,       // Capture video/frames
    PROCESS,    // Pose estimation + preprocessing
    MEASURE,    // Calculate angles + ROM
    REPORT      // Generate report
};

struct MeasurementResult {
    std::string movement;
    std::string side;
    float angle_a = 0.0f;  // Resting
    float angle_b = 0.0f;  // Max ROM
    float rom = 0.0f;
    std::string classification;
};

class MeasurementPipeline {
public:
    MeasurementPipeline();

    void start_session(const std::string& patient_id);
    void capture_frame(const std::string& phase);  // "A" or "B"
    void add_measurement(const std::string& movement, const std::string& side,
                         float angle_a, float angle_b);
    MeasurementResult get_result(const std::string& movement, const std::string& side) const;
    std::vector<MeasurementResult> get_all_results() const;
    std::string generate_report() const;

    void set_stage(MeasurementStage stage);
    MeasurementStage get_stage() const;

    MarkerManager& markers() { return marker_mgr_; }
    ROIManager& rois() { return roi_mgr_; }

private:
    std::string patient_id_;
    std::string session_id_;
    MeasurementStage stage_;
    std::vector<MeasurementResult> results_;
    MarkerManager marker_mgr_;
    ROIManager roi_mgr_;
};

} // namespace oculus