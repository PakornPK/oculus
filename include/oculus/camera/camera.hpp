#pragma once

#include "oculus/core/frame.hpp"
#include <string>

namespace oculus {

struct CameraInfo {
    std::string name;
    std::string device_path;
    int max_width = 0;
    int max_height = 0;
    int max_fps = 0;
    bool has_depth = false;
};

class Camera {
public:
    virtual ~Camera() = default;
    virtual bool open() = 0;
    virtual void close() = 0;
    virtual Frame capture() = 0;
    virtual bool is_open() const = 0;
    virtual CameraInfo info() const = 0;

    virtual bool has_depth() const { return false; }
};

} // namespace oculus