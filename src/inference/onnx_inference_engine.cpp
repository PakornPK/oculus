#include "oculus/inference/onnx_inference_engine.hpp"
#include "oculus/inference/models/rtmpose/rtmpose_preprocessor.hpp"
#include "oculus/inference/models/rtmpose/rtmpose_postprocessor.hpp"
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

    RTMPosePreprocessor preprocessor;
    RTMPosePostprocessor postprocessor;
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

    // Use center crop preprocessing
    std::vector<float> input_tensor = impl_->preprocessor.process(frame);

    std::vector<int64_t> input_shape = {1, 3,
        RTMPosePreprocessor::INPUT_HEIGHT,
        RTMPosePreprocessor::INPUT_WIDTH};

    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(
        OrtArenaAllocator, OrtMemTypeDefault);

    Ort::Value input_ort_tensor = Ort::Value::CreateTensor<float>(
        memory_info, input_tensor.data(), input_tensor.size(),
        input_shape.data(), input_shape.size());

    std::vector<const char*> input_names_cstr;
    for (const auto& name : impl_->input_names) {
        input_names_cstr.push_back(name.c_str());
    }
    std::vector<const char*> output_names_cstr;
    for (const auto& name : impl_->output_names) {
        output_names_cstr.push_back(name.c_str());
    }

    Ort::RunOptions run_options;
    auto output_tensors = impl_->session->Run(
        run_options,
        input_names_cstr.data(), &input_ort_tensor, 1,
        output_names_cstr.data(), output_names_cstr.size());

    auto& simcc_x_tensor = output_tensors[0];
    auto& simcc_y_tensor = output_tensors[1];

    float* simcc_x_data = simcc_x_tensor.GetTensorMutableData<float>();
    float* simcc_y_data = simcc_y_tensor.GetTensorMutableData<float>();

    auto simcc_x_shape = simcc_x_tensor.GetTensorTypeAndShapeInfo().GetShape();
    auto simcc_y_shape = simcc_y_tensor.GetTensorTypeAndShapeInfo().GetShape();

    size_t simcc_x_size = 1, simcc_y_size = 1;
    for (auto dim : simcc_x_shape) if (dim > 0) simcc_x_size *= static_cast<size_t>(dim);
    for (auto dim : simcc_y_shape) if (dim > 0) simcc_y_size *= static_cast<size_t>(dim);

    std::vector<float> simcc_x(simcc_x_data, simcc_x_data + simcc_x_size);
    std::vector<float> simcc_y(simcc_y_data, simcc_y_data + simcc_y_size);

    return impl_->postprocessor.process_simcc(simcc_x, simcc_y, frame.timestamp);
}

PoseResult OnnxInferenceEngine::infer_with_detection(
    const Frame& frame, PersonDetector& detector) {

    if (!impl_->model_loaded) {
        throw std::runtime_error("Model not loaded");
    }

    // Detect person first
    auto persons = detector.detect(frame, 0.5f);

    if (persons.empty()) {
        // No person detected - return empty result
        PoseResult result;
        result.timestamp = frame.timestamp;
        return result;
    }

    // Use largest person
    const auto& person = persons[0];

    // Preprocess with person bbox
    std::vector<float> input_tensor = impl_->preprocessor.process_with_bbox(
        frame, person.x1, person.y1, person.x2, person.y2);

    std::vector<int64_t> input_shape = {1, 3,
        RTMPosePreprocessor::INPUT_HEIGHT,
        RTMPosePreprocessor::INPUT_WIDTH};

    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(
        OrtArenaAllocator, OrtMemTypeDefault);

    Ort::Value input_ort_tensor = Ort::Value::CreateTensor<float>(
        memory_info, input_tensor.data(), input_tensor.size(),
        input_shape.data(), input_shape.size());

    std::vector<const char*> input_names_cstr;
    for (const auto& name : impl_->input_names) {
        input_names_cstr.push_back(name.c_str());
    }
    std::vector<const char*> output_names_cstr;
    for (const auto& name : impl_->output_names) {
        output_names_cstr.push_back(name.c_str());
    }

    Ort::RunOptions run_options;
    auto output_tensors = impl_->session->Run(
        run_options,
        input_names_cstr.data(), &input_ort_tensor, 1,
        output_names_cstr.data(), output_names_cstr.size());

    auto& simcc_x_tensor = output_tensors[0];
    auto& simcc_y_tensor = output_tensors[1];

    float* simcc_x_data = simcc_x_tensor.GetTensorMutableData<float>();
    float* simcc_y_data = simcc_y_tensor.GetTensorMutableData<float>();

    auto simcc_x_shape = simcc_x_tensor.GetTensorTypeAndShapeInfo().GetShape();
    auto simcc_y_shape = simcc_y_tensor.GetTensorTypeAndShapeInfo().GetShape();

    size_t simcc_x_size = 1, simcc_y_size = 1;
    for (auto dim : simcc_x_shape) if (dim > 0) simcc_x_size *= static_cast<size_t>(dim);
    for (auto dim : simcc_y_shape) if (dim > 0) simcc_y_size *= static_cast<size_t>(dim);

    std::vector<float> simcc_x(simcc_x_data, simcc_x_data + simcc_x_size);
    std::vector<float> simcc_y(simcc_y_data, simcc_y_data + simcc_y_size);

    auto result = impl_->postprocessor.process_simcc(simcc_x, simcc_y, frame.timestamp);

    // Scale keypoints back to original frame coordinates
    // The preprocessor crops around the person bbox, so we need to map back
    float bbox_w = person.x2 - person.x1;
    float bbox_h = person.y2 - person.y1;
    float expand_x = bbox_w * 0.2f;
    float expand_y = bbox_h * 0.2f;

    int crop_x = static_cast<int>(std::max(0.0f, person.x1 - expand_x));
    int crop_y = static_cast<int>(std::max(0.0f, person.y1 - expand_y));

    // Scale from model input space to crop space
    float model_aspect = static_cast<float>(RTMPosePreprocessor::INPUT_WIDTH) /
                          RTMPosePreprocessor::INPUT_HEIGHT;
    float crop_aspect = (bbox_w + 2 * expand_x) / (bbox_h + 2 * expand_y);

    float scale_x, scale_y;
    if (crop_aspect > model_aspect) {
        scale_x = static_cast<float>(crop_x + static_cast<int>(bbox_w + 2 * expand_x)) /
                  RTMPosePreprocessor::INPUT_WIDTH;
        scale_y = scale_x;
    } else {
        scale_y = static_cast<float>(crop_y + static_cast<int>(bbox_h + 2 * expand_y)) /
                  RTMPosePreprocessor::INPUT_HEIGHT;
        scale_x = scale_y;
    }

    for (auto& pose : result.poses) {
        for (auto& kp : pose.keypoints) {
            kp.x = kp.x * scale_x + crop_x;
            kp.y = kp.y * scale_y + crop_y;
        }
    }

    return result;
}

std::string OnnxInferenceEngine::backend_name() const {
    return impl_->backend;
}

bool OnnxInferenceEngine::is_model_loaded() const {
    return impl_->model_loaded;
}

} // namespace oculus