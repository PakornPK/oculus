// tests/mocks/mock_camera.hpp
// Mock camera สำหรับทดสอบ pipeline โดยไม่ต้องใช้ hardware จริง

#pragma once

#include "oculus/camera/camera.hpp"
#include <queue>
#include <mutex>
#include <vector>

namespace oculus {
namespace testing {

class MockCamera : public Camera {
public:
    MockCamera() = default;
    ~MockCamera() override = default;
    
    // Inject test frames
    void enqueue_frame(const Frame& frame) {
        std::lock_guard<std::mutex> lock(mutex_);
        frame_queue_.push(frame);
    }
    
    void enqueue_frames(const std::vector<Frame>& frames) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& frame : frames) {
            frame_queue_.push(frame);
        }
    }
    
    // Camera interface implementation
    bool open() override {
        if (fail_on_open_) {
            return false;
        }
        is_open_ = true;
        return true;
    }
    
    void close() override {
        is_open_ = false;
    }
    
    Frame capture() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!is_open_) {
            throw std::runtime_error("Camera not open");
        }
        
        if (frame_queue_.empty()) {
            if (loop_frames_) {
                // วน loop กลับไปใช้ frames เดิม
                reset_queue();
            } else {
                throw std::runtime_error("No frames available");
            }
        }
        
        Frame frame = frame_queue_.front();
        frame_queue_.pop();
        return frame;
    }
    
    bool is_open() const override {
        return is_open_;
    }
    
    // Test helpers
    void set_fail_on_open(bool fail) { fail_on_open_ = fail; }
    void set_loop_frames(bool loop) { loop_frames_ = loop; }
    
    size_t frames_remaining() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return frame_queue_.size();
    }
    
    void clear_frames() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!frame_queue_.empty()) {
            frame_queue_.pop();
        }
    }
    
private:
    void reset_queue() {
        // ไม่ lock เพราะ caller 已經 lock แล้ว
        // ใช้ backup frames
    }
    
    bool is_open_ = false;
    bool fail_on_open_ = false;
    bool loop_frames_ = false;
    
    mutable std::mutex mutex_;
    std::queue<Frame> frame_queue_;
    std::vector<Frame> backup_frames_;
};

// Helper สร้าง test frame
inline Frame create_test_frame(int width = 640, int height = 480, 
                                PixelFormat format = PixelFormat::RGB) {
    Frame frame;
    frame.width = width;
    frame.height = height;
    frame.format = format;
    frame.timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    frame.frame_id = 0;
    
    int channels = (format == PixelFormat::GRAY) ? 1 : 3;
    frame.data.resize(width * height * channels);
    
    // Fill with gradient pattern
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < channels; ++c) {
                frame.data[(y * width + x) * channels + c] = 
                    static_cast<uint8_t>((x + y) % 256);
            }
        }
    }
    
    return frame;
}

// Helper สร้าง test frame with specific pixel value
inline Frame create_uniform_frame(int width, int height, uint8_t value,
                                   PixelFormat format = PixelFormat::RGB) {
    Frame frame;
    frame.width = width;
    frame.height = height;
    frame.format = format;
    frame.timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    frame.frame_id = 0;
    
    int channels = (format == PixelFormat::GRAY) ? 1 : 3;
    frame.data.assign(width * height * channels, value);
    
    return frame;
}

} // namespace testing
} // namespace oculus
