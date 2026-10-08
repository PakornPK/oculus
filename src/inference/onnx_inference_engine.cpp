#include "oculus/inference/onnx_inference_engine.hpp"
#include <spdlog/spdlog.h>
#include <onnxruntime_cxx_api.h>

#include <stdexcept>
#include <vector>

namespace oculus {

struct OnnxInferenceEngine::Impl {
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "oculus"};
    Ort::SessionOptions session_options;
    std::unique_ptr<Ort::Session> session;
    Ort::AllocatorWithDefaultOptions allocator;
    std::string backend = "cpu";
    bool model_loaded = false;

    std::vector<std::string> input_names;
    std::vector<std::string> output_names;
    std::vector<std::vector<int64_t>> input_shapes;
    std::vector<std::vector<int64_t>> output_shapes;
};

OnnxInferenceEngine::OnnxInferenceEngine()
    : impl_(std::make_unique<Impl>()) {
    impl_->session_options.SetGraphOptimizationLevel(ORT_ENABLE_ALL);
    impl_->session_options.SetIntraOpNumThreads(4);
}

OnnxInferenceEngine::~OnnxInferenceEngine() = default;

bool OnnxInferenceEngine::load_model(const std::string& model_path) {
    try {
        spdlog::info("Loading ONNX model: {}", model_path);
        impl_->session = std::make_unique<Ort::Session>(
            impl_->env, model_path.c_str(), impl_->session_options);

        size_t input_count = impl_->session->GetInputCount();
        size_t output_count = impl_->session->GetOutputCount();

        impl_->input_names.clear();
        impl_->output_names.clear();
        impl_->input_shapes.clear();
        impl_->output_shapes.clear();

        for (size_t i = 0; i < input_count; ++i) {
            auto name = impl_->session->GetInputNameAllocated(i, impl_->allocator);
            impl_->input_names.push_back(name.get());

            auto type_info = impl_->session->GetInputTypeInfo(i);
            auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
            impl_->input_shapes.push_back(tensor_info.GetShape());
        }

        for (size_t i = 0; i < output_count; ++i) {
            auto name = impl_->session->GetOutputNameAllocated(i, impl_->allocator);
            impl_->output_names.push_back(name.get());

            auto type_info = impl_->session->GetOutputTypeInfo(i);
            auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
            impl_->output_shapes.push_back(tensor_info.GetShape());
        }

        impl_->model_loaded = true;
        spdlog::info("Model loaded: {} inputs, {} outputs", input_count, output_count);
        return true;

    } catch (const Ort::Exception& e) {
        spdlog::error("Failed to load model: {}", e.what());
        impl_->model_loaded = false;
        return false;
    }
}

PoseResult OnnxInferenceEngine::infer(const Frame& frame) {
    if (!impl_->model_loaded) {
        throw std::runtime_error("Model not loaded");
    }

    PoseResult result;
    result.timestamp = frame.timestamp;

    // Stub: real preprocessing + inference + postprocessing will be added
    // in Phase 6A when RTMPose preprocessor/postprocessor are implemented.
    // For now, return empty result to prove the pipeline compiles and links.

    return result;
}

std::string OnnxInferenceEngine::backend_name() const {
    return impl_->backend;
}

bool OnnxInferenceEngine::is_model_loaded() const {
    return impl_->model_loaded;
}

} // namespace oculus