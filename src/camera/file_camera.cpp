#include "oculus/camera/file_camera.hpp"
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace oculus {

FileCamera::FileCamera(const std::string& file_path)
    : file_path_(file_path) {}

FileCamera::~FileCamera() {
    close();
}

bool FileCamera::open() {
    if (file_path_.empty()) {
        spdlog::error("FileCamera: empty file path");
        return false;
    }

    spdlog::info("FileCamera: opening {}", file_path_);
    is_open_ = true;
    current_frame_ = 0;
    return true;
}

void FileCamera::close() {
    is_open_ = false;
    current_frame_ = 0;
}

Frame FileCamera::capture() {
    if (!is_open_) {
        throw std::runtime_error("FileCamera: not open");
    }

    Frame frame;
    frame.width = 640;
    frame.height = 480;
    frame.format = PixelFormat::RGB;
    frame.frame_id = current_frame_++;
    frame.timestamp = current_frame_ * 33;

    frame.data.resize(frame.width * frame.height * 3, 0);

    if (loop_ && total_frames_ > 0 && current_frame_ >= total_frames_) {
        current_frame_ = 0;
    }

    return frame;
}

bool FileCamera::is_open() const {
    return is_open_;
}

CameraInfo FileCamera::info() const {
    CameraInfo cam_info;
    cam_info.name = "FileCamera";
    cam_info.device_path = file_path_;
    cam_info.max_width = 640;
    cam_info.max_height = 480;
    cam_info.max_fps = 30;
    cam_info.has_depth = false;
    return cam_info;
}

void FileCamera::set_loop(bool loop) {
    loop_ = loop;
}

int FileCamera::total_frames() const {
    return total_frames_;
}

int FileCamera::current_frame() const {
    return current_frame_;
}

} // namespace oculus