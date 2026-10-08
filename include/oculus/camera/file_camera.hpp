#pragma once

#include "oculus/camera/camera.hpp"
#include <string>

namespace oculus {

class FileCamera : public Camera {
public:
    explicit FileCamera(const std::string& file_path);
    ~FileCamera() override;

    FileCamera(const FileCamera&) = delete;
    FileCamera& operator=(const FileCamera&) = delete;

    bool open() override;
    void close() override;
    Frame capture() override;
    bool is_open() const override;
    CameraInfo info() const override;

    void set_loop(bool loop);
    bool seek(int frame_number);
    int total_frames() const;
    int current_frame() const;

private:
    std::string file_path_;
    void* cap_ = nullptr;
    bool is_open_ = false;
    bool loop_ = false;
    int total_frames_ = 0;
    int current_frame_ = 0;
    int width_ = 0;
    int height_ = 0;
    double fps_ = 0.0;
};

} // namespace oculus