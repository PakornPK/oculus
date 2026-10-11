#include "oculus/camera/camera_factory.hpp"
#include "oculus/camera/file_camera.hpp"
#include "oculus/camera/opencv_camera.hpp"
#include <spdlog/spdlog.h>

namespace oculus {

std::unique_ptr<Camera> CameraFactory::create(const CameraConfig& config) {
    if (config.device == "file") {
        spdlog::warn("CameraFactory: 'file' device requires path, use create_file_camera()");
        return nullptr;
    }

    // Webcam: "0", "1", "webcam", "auto"
    int device_id = -1;
    if (config.device == "auto" || config.device == "webcam") {
        device_id = 0;
    } else {
        try {
            device_id = std::stoi(config.device);
        } catch (...) {
            device_id = -1;
        }
    }

    if (device_id >= 0) {
        auto cam = std::make_unique<OpenCVCamera>(device_id);
        cam->set_resolution(config.width, config.height);
        cam->set_fps(config.fps);
        if (cam->open()) {
            spdlog::info("CameraFactory: opened webcam device {}", device_id);
            return cam;
        }
        spdlog::warn("CameraFactory: failed to open webcam device {}", device_id);
        return nullptr;
    }

    spdlog::error("CameraFactory: unknown device '{}'", config.device);
    return nullptr;
}

std::unique_ptr<Camera> CameraFactory::create_file_camera(const std::string& path) {
    auto camera = std::make_unique<FileCamera>(path);
    spdlog::info("CameraFactory: created FileCamera for {}", path);
    return camera;
}

} // namespace oculus