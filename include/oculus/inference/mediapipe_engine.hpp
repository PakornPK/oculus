#pragma once

#include "oculus/inference/inference_engine.hpp"
#include "oculus/inference/person_detector.hpp"
#include <memory>
#include <string>

namespace oculus {

class MediaPipeEngine : public InferenceEngine {
public:
    MediaPipeEngine();
    ~MediaPipeEngine() override;

    bool load_model(const std::string& model_path) override;
    PoseResult infer(const Frame& frame) override;
    std::string backend_name() const override;
    bool is_model_loaded() const override;

    /// Infer on a pre-cropped region (faster: less data over pipe).
    /// Keypoints are mapped back to original frame coordinates.
    PoseResult infer_crop(const Frame& frame, const PersonBox& box);

    /// Start the worker subprocess. Call once after load_model.
    bool start_worker();

    /// Stop the worker subprocess.
    void stop_worker();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace oculus