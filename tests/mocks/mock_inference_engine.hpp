// tests/mocks/mock_inference_engine.hpp
// Mock inference engine สำหรับทดสอบ pipeline โดยไม่ต้องโหลด ONNX model

#pragma once

#include "oculus/inference/inference_engine.hpp"
#include <functional>
#include <queue>

namespace oculus {
namespace testing {

class MockInferenceEngine : public InferenceEngine {
public:
    MockInferenceEngine() = default;
    ~MockInferenceEngine() override = default;
    
    // Inject test results
    void enqueue_result(const PoseResult& result) {
        result_queue_.push(result);
    }
    
    void set_result_generator(std::function<PoseResult(const Frame&)> generator) {
        result_generator_ = generator;
    }
    
    // InferenceEngine interface
    bool load_model(const std::string& model_path) override {
        if (fail_on_load_) {
            last_error_ = "Mock: Failed to load model";
            return false;
        }
        model_loaded_ = true;
        model_path_ = model_path;
        return true;
    }
    
    PoseResult infer(const Frame& frame) override {
        if (!model_loaded_) {
            throw std::runtime_error("Model not loaded");
        }
        
        inference_count_++;
        
        // ใช้ custom generator ถ้ามี
        if (result_generator_) {
            return result_generator_(frame);
        }
        
        // ใช้ pre-queued results
        if (!result_queue_.empty()) {
            PoseResult result = result_queue_.front();
            result_queue_.pop();
            return result;
        }
        
        // Default: return empty result
        PoseResult result;
        result.timestamp = frame.timestamp;
        return result;
    }
    
    std::string backend_name() const override {
        return "mock";
    }
    
    bool is_model_loaded() const override {
        return model_loaded_;
    }
    
    // Test helpers
    void set_fail_on_load(bool fail) { fail_on_load_ = fail; }
    
    int inference_count() const { return inference_count_; }
    
    void reset_counters() { inference_count_ = 0; }
    
    std::string last_error() const { return last_error_; }
    
private:
    bool model_loaded_ = false;
    bool fail_on_load_ = false;
    int inference_count_ = 0;
    std::string model_path_;
    std::string last_error_;
    
    std::queue<PoseResult> result_queue_;
    std::function<PoseResult(const Frame&)> result_generator_;
};

// Helper สร้าง test pose result
inline PoseResult create_test_pose_result(int num_poses = 1, 
                                           int num_keypoints = 17) {
    PoseResult result;
    result.timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    
    for (int p = 0; p < num_poses; ++p) {
        Pose pose;
        pose.confidence = 0.9f;
        
        for (int k = 0; k < num_keypoints; ++k) {
            Keypoint kp;
            kp.x = 100.0f + k * 50.0f;  // spread keypoints
            kp.y = 200.0f + (k % 3) * 30.0f;
            kp.confidence = 0.8f + (k % 5) * 0.04f;  // 0.8 - 0.96
            pose.keypoints.push_back(kp);
        }
        
        result.poses.push_back(pose);
    }
    
    return result;
}

// Helper สร้าง test pose result with specific keypoint positions
inline PoseResult create_pose_with_keypoints(
    const std::vector<std::pair<float, float>>& positions) {
    
    PoseResult result;
    result.timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    
    Pose pose;
    pose.confidence = 0.95f;
    
    for (const auto& [x, y] : positions) {
        Keypoint kp;
        kp.x = x;
        kp.y = y;
        kp.confidence = 0.9f;
        pose.keypoints.push_back(kp);
    }
    
    result.poses.push_back(pose);
    return result;
}

} // namespace testing
} // namespace oculus
