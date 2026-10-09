#include "oculus/inference/person_detector.hpp"
#include <onnxruntime_cxx_api.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>

namespace oculus {

struct PersonDetector::Impl {
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "oculus-det"};
    Ort::SessionOptions session_options;
    std::unique_ptr<Ort::Session> session;
    Ort::AllocatorWithDefaultOptions allocator;
    bool loaded = false;

    static constexpr int INPUT_SIZE = 640;
};

PersonDetector::PersonDetector() : impl_(std::make_unique<Impl>()) {
    impl_->session_options.SetGraphOptimizationLevel(ORT_ENABLE_ALL);
    impl_->session_options.SetIntraOpNumThreads(2);
}

PersonDetector::~PersonDetector() = default;

bool PersonDetector::load_model(const std::string& model_path) {
    try {
        spdlog::info("Loading person detector: {}", model_path);
        impl_->session = std::make_unique<Ort::Session>(
            impl_->env, model_path.c_str(), impl_->session_options);
        impl_->loaded = true;
        spdlog::info("Person detector loaded");
        return true;
    } catch (const Ort::Exception& e) {
        spdlog::error("Failed to load person detector: {}", e.what());
        impl_->loaded = false;
        return false;
    }
}

std::vector<PersonBox> PersonDetector::detect(const Frame& frame, float confidence_threshold) {
    std::vector<PersonBox> results;
    if (!impl_->loaded || !frame.is_valid()) return results;

    cv::Mat rgb(frame.height, frame.width, CV_8UC3,
                const_cast<uint8_t*>(frame.data.data()));

    // Convert RGB to BGR (YOLOX expects OpenCV BGR format)
    cv::Mat bgr;
    cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);

    // Letterbox resize to 640x640
    float scale = std::min(
        static_cast<float>(impl_->INPUT_SIZE) / frame.width,
        static_cast<float>(impl_->INPUT_SIZE) / frame.height);

    int new_w = static_cast<int>(frame.width * scale);
    int new_h = static_cast<int>(frame.height * scale);
    int pad_x = (impl_->INPUT_SIZE - new_w) / 2;
    int pad_y = (impl_->INPUT_SIZE - new_h) / 2;

    cv::Mat resized;
    cv::resize(bgr, resized, cv::Size(new_w, new_h));

    cv::Mat padded(impl_->INPUT_SIZE, impl_->INPUT_SIZE, CV_8UC3, cv::Scalar(114, 114, 114));
    resized.copyTo(padded(cv::Rect(pad_x, pad_y, new_w, new_h)));

    // Normalize: /255.0, HWC->CHW (BGR order)
    std::vector<float> input_tensor(1 * 3 * impl_->INPUT_SIZE * impl_->INPUT_SIZE);
    const int hw = impl_->INPUT_SIZE * impl_->INPUT_SIZE;
    for (int c = 0; c < 3; ++c) {
        for (int h = 0; h < impl_->INPUT_SIZE; ++h) {
            for (int w = 0; w < impl_->INPUT_SIZE; ++w) {
                input_tensor[c * hw + h * impl_->INPUT_SIZE + w] =
                    padded.at<cv::Vec3b>(h, w)[c] / 255.0f;
            }
        }
    }

    // Run inference
    std::vector<int64_t> input_shape = {1, 3, impl_->INPUT_SIZE, impl_->INPUT_SIZE};
    Ort::MemoryInfo mem_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value input_val = Ort::Value::CreateTensor<float>(
        mem_info, input_tensor.data(), input_tensor.size(),
        input_shape.data(), input_shape.size());

    auto input_name = impl_->session->GetInputNameAllocated(0, impl_->allocator);
    const char* input_names[] = {input_name.get()};

    Ort::RunOptions run_options;

    // Get output names (YOLOX has 2 outputs: dets + labels)
    auto det_name = impl_->session->GetOutputNameAllocated(0, impl_->allocator);
    auto label_name = impl_->session->GetOutputNameAllocated(1, impl_->allocator);
    const char* out_names[] = {det_name.get(), label_name.get()};

    auto outputs = impl_->session->Run(run_options, input_names, &input_val, 1, out_names, 2);

    // Parse YOLOX output:
    //   dets: [1, N, 5] (x1, y1, x2, y2, score)
    //   labels: [1, N] (class_id)
    auto& dets = outputs[0];
    auto& labels = outputs[1];

    auto det_shape = dets.GetTensorTypeAndShapeInfo().GetShape();
    float* det_data = dets.GetTensorMutableData<float>();
    int64_t* label_data = labels.GetTensorMutableData<int64_t>();

    int num_detections = static_cast<int>(det_shape[1]);

    // Debug: log raw output
    spdlog::info("YOLOX: {} detections, output shape=[{}, {}, {}]",
                 num_detections, det_shape[0], det_shape[1], det_shape[2]);
    if (num_detections > 0) {
        spdlog::info("YOLOX: det[0]=({:.1f},{:.1f},{:.1f},{:.1f}) score={:.4f} label={}",
                     det_data[0], det_data[1], det_data[2], det_data[3],
                     det_data[4], label_data[0]);
    }

    for (int i = 0; i < num_detections; ++i) {
        float* det = det_data + i * 5;
        float score = det[4];
        int64_t class_id = label_data[i];

        if (score >= confidence_threshold && class_id == 0) {  // class 0 = person
            PersonBox box;
            // Convert from letterbox coords back to original image coords
            box.x1 = (det[0] - pad_x) / scale;
            box.y1 = (det[1] - pad_y) / scale;
            box.x2 = (det[2] - pad_x) / scale;
            box.y2 = (det[3] - pad_y) / scale;
            box.confidence = score;

            // Clamp to image bounds
            box.x1 = std::max(0.0f, std::min(box.x1, static_cast<float>(frame.width)));
            box.y1 = std::max(0.0f, std::min(box.y1, static_cast<float>(frame.height)));
            box.x2 = std::max(0.0f, std::min(box.x2, static_cast<float>(frame.width)));
            box.y2 = std::max(0.0f, std::min(box.y2, static_cast<float>(frame.height)));

            results.push_back(box);
        }
    }

    // Sort by confidence (highest first)
    std::sort(results.begin(), results.end(),
              [](const PersonBox& a, const PersonBox& b) {
                  return a.confidence > b.confidence;
              });

    return results;
}

bool PersonDetector::is_loaded() const {
    return impl_->loaded;
}

} // namespace oculus