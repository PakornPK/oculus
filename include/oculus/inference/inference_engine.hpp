#pragma once

#include "oculus/core/frame.hpp"
#include "oculus/core/pose.hpp"
#include <string>

namespace oculus {

class InferenceEngine {
public:
    virtual ~InferenceEngine() = default;
    virtual bool load_model(const std::string& model_path) = 0;
    virtual PoseResult infer(const Frame& frame) = 0;
    virtual std::string backend_name() const = 0;
    virtual bool is_model_loaded() const = 0;
};

} // namespace oculus