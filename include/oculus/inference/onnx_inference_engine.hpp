#pragma once

#include "oculus/inference/inference_engine.hpp"

#include <memory>
#include <string>

namespace oculus {

class OnnxInferenceEngine : public InferenceEngine {
public:
    OnnxInferenceEngine();
    ~OnnxInferenceEngine() override;

    bool load_model(const std::string& model_path) override;
    PoseResult infer(const Frame& frame) override;
    std::string backend_name() const override;
    bool is_model_loaded() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace oculus