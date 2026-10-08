#include "oculus/camera/file_camera.hpp"
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace oculus {

FileCamera::FileCamera(const std::string& file_path)
    : file_path_(file_path), cap_(new cv::VideoCapture()) {}

FileCamera::~FileCamera() {
    close();
    delete static_cast<cv::VideoCapture*>(cap_);
}

bool FileCamera::open() {
    if (file_path_.empty()) {
        spdlog::error("FileCamera: empty file path");
        return false;
    }

    auto* cap = static_cast<cv::VideoCapture*>(cap_);
    spdlog::info("FileCamera: opening {}", file_path_);

    cap->open(file_path_);
    if (!cap->isOpened()) {
        spdlog::error("FileCamera: failed to open {}", file_path_);
        return false;
    }

    width_ = static_cast<int>(cap->get(cv::CAP_PROP_FRAME_WIDTH));
    height_ = static_cast<int>(cap->get(cv::CAP_PROP_FRAME_HEIGHT));
    fps_ = cap->get(cv::CAP_PROP_FPS);
    total_frames_ = static_cast<int>(cap->get(cv::CAP_PROP_FRAME_COUNT));

    if (width_ <= 0 || height_ <= 0) {
        spdlog::error("FileCamera: invalid video dimensions {}x{}", width_, height_);
        cap->release();
        return false;
    }

    current_frame_ = 0;
    is_open_ = true;

    spdlog::info("FileCamera: {}x{} @ {:.1f} fps, {} frames",
                 width_, height_, fps_, total_frames_);
    return true;
}

void FileCamera::close() {
    auto* cap = static_cast<cv::VideoCapture*>(cap_);
    if (cap->isOpened()) {
        cap->release();
    }
    is_open_ = false;
    current_frame_ = 0;
}

Frame FileCamera::capture() {
    if (!is_open_) {
        throw std::runtime_error("FileCamera: not open");
    }

    auto* cap = static_cast<cv::VideoCapture*>(cap_);

    cv::Mat bgr_frame;
    if (!cap->read(bgr_frame)) {
        if (loop_ && total_frames_ > 0) {
            cap->set(cv::CAP_PROP_POS_FRAMES, 0);
            current_frame_ = 0;
            if (!cap->read(bgr_frame)) {
                throw std::runtime_error("FileCamera: failed to read frame after loop reset");
            }
        } else {
            throw std::runtime_error("FileCamera: end of video");
        }
    }

    cv::Mat rgb_frame;
    cv::cvtColor(bgr_frame, rgb_frame, cv::COLOR_BGR2RGB);

    Frame frame;
    frame.width = rgb_frame.cols;
    frame.height = rgb_frame.rows;
    frame.format = PixelFormat::RGB;
    frame.frame_id = current_frame_;
    frame.timestamp = static_cast<int64_t>(
        (static_cast<double>(current_frame_) / (fps_ > 0 ? fps_ : 30.0)) * 1000.0);

    const size_t data_size = static_cast<size_t>(rgb_frame.total()) * rgb_frame.elemSize();
    frame.data.assign(rgb_frame.data, rgb_frame.data + data_size);

    current_frame_++;

    return frame;
}

bool FileCamera::is_open() const {
    return is_open_;
}

CameraInfo FileCamera::info() const {
    CameraInfo cam_info;
    cam_info.name = "FileCamera";
    cam_info.device_path = file_path_;
    cam_info.max_width = width_;
    cam_info.max_height = height_;
    cam_info.max_fps = static_cast<int>(fps_);
    cam_info.has_depth = false;
    return cam_info;
}

void FileCamera::set_loop(bool loop) {
    loop_ = loop;
}

bool FileCamera::seek(int frame_number) {
    if (!is_open_) {
        return false;
    }
    if (frame_number < 0 || (total_frames_ > 0 && frame_number >= total_frames_)) {
        return false;
    }
    auto* cap = static_cast<cv::VideoCapture*>(cap_);
    cap->set(cv::CAP_PROP_POS_FRAMES, frame_number);
    current_frame_ = frame_number;
    return true;
}

int FileCamera::total_frames() const {
    return total_frames_;
}

int FileCamera::current_frame() const {
    return current_frame_;
}

} // namespace oculus