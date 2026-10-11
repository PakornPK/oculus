#pragma once

#include "oculus/camera/camera.hpp"
#include <memory>
#include <string>

namespace oculus {

/// Camera that captures from a webcam using OpenCV VideoCapture.
/// Works on macOS (AVFoundation), Linux (V4L2), and Windows (DSHOW).
class OpenCVCamera : public Camera {
public:
    explicit OpenCVCamera(int device_id = 0);
    ~OpenCVCamera() override;

    bool open() override;
    void close() override;
    bool is_open() const override;
    Frame capture() override;
    CameraInfo info() const override;

    void set_resolution(int width, int height);
    void set_fps(int fps);

private:
    int device_id_;
    int width_ = 640;
    int height_ = 480;
    int fps_ = 30;
    void* cap_ = nullptr;  // cv::VideoCapture*
    bool is_open_ = false;
    int64_t frame_count_ = 0;
};

} // namespace oculus