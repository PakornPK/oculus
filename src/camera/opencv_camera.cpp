#include "oculus/camera/opencv_camera.hpp"
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>
#include <spdlog/spdlog.h>

namespace oculus {

OpenCVCamera::OpenCVCamera(int device_id)
    : device_id_(device_id) {}

OpenCVCamera::~OpenCVCamera() {
    close();
}

bool OpenCVCamera::open() {
    if (is_open_) return true;

    auto* cap = new cv::VideoCapture(device_id_);
    if (!cap->isOpened()) {
        spdlog::error("OpenCVCamera: failed to open device {}", device_id_);
        delete cap;
        return false;
    }

    cap->set(cv::CAP_PROP_FRAME_WIDTH, width_);
    cap->set(cv::CAP_PROP_FRAME_HEIGHT, height_);
    cap->set(cv::CAP_PROP_FPS, fps_);

    // Read actual values
    width_ = static_cast<int>(cap->get(cv::CAP_PROP_FRAME_WIDTH));
    height_ = static_cast<int>(cap->get(cv::CAP_PROP_FRAME_HEIGHT));
    fps_ = static_cast<int>(cap->get(cv::CAP_PROP_FPS));

    cap_ = cap;
    is_open_ = true;
    spdlog::info("OpenCVCamera: opened device {} ({}x{} @ {}fps)",
                 device_id_, width_, height_, fps_);
    return true;
}

void OpenCVCamera::close() {
    if (cap_) {
        delete static_cast<cv::VideoCapture*>(cap_);
        cap_ = nullptr;
    }
    is_open_ = false;
}

bool OpenCVCamera::is_open() const {
    return is_open_;
}

Frame OpenCVCamera::capture() {
    Frame frame;
    if (!is_open_ || !cap_) return frame;

    auto* cap = static_cast<cv::VideoCapture*>(cap_);
    cv::Mat bgr;
    if (!cap->read(bgr) || bgr.empty()) {
        spdlog::warn("OpenCVCamera: failed to capture frame");
        return frame;
    }

    // Convert BGR → RGB
    cv::Mat rgb;
    cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);

    frame.width = rgb.cols;
    frame.height = rgb.rows;
    frame.format = PixelFormat::RGB;
    frame.data.assign(rgb.datastart, rgb.dataend);
    frame.timestamp = frame_count_ * 33;  // ~30fps
    frame.frame_id = frame_count_++;

    return frame;
}

CameraInfo OpenCVCamera::info() const {
    CameraInfo ci;
    ci.name = "OpenCV Camera " + std::to_string(device_id_);
    ci.max_width = width_;
    ci.max_height = height_;
    ci.max_fps = fps_;
    return ci;
}

void OpenCVCamera::set_resolution(int width, int height) {
    width_ = width;
    height_ = height;
}

void OpenCVCamera::set_fps(int fps) {
    fps_ = fps;
}

} // namespace oculus