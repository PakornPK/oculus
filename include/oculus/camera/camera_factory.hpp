#pragma once

#include "oculus/camera/camera.hpp"
#include <memory>
#include <string>

namespace oculus {

struct CameraConfig {
    std::string device = "auto";
    int width = 640;
    int height = 480;
    int fps = 30;
};

class CameraFactory {
public:
    static std::unique_ptr<Camera> create(const CameraConfig& config);
    static std::unique_ptr<Camera> create_file_camera(const std::string& path);
};

} // namespace oculus