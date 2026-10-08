#include "oculus/camera/camera_factory.hpp"
#include "oculus/camera/file_camera.hpp"
#include <spdlog/spdlog.h>

namespace oculus {

std::unique_ptr<Camera> CameraFactory::create(const CameraConfig& config) {
    if (config.device == "file") {
        spdlog::warn("CameraFactory: 'file' device requires path, use create_file_camera()");
        return nullptr;
    }

    if (config.device == "auto") {
        spdlog::info("CameraFactory: auto-detect not implemented, returning nullptr");
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