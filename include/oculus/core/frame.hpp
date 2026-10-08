#pragma once

#include <cstdint>
#include <vector>
#include <chrono>

namespace oculus {

enum class PixelFormat {
    RGB,
    BGR,
    GRAY,
    DEPTH
};

struct Frame {
    std::vector<uint8_t> data;
    int width = 0;
    int height = 0;
    PixelFormat format = PixelFormat::RGB;
    int64_t timestamp = 0;
    int64_t frame_id = 0;

    int channels() const {
        return (format == PixelFormat::GRAY) ? 1 : 3;
    }

    size_t expected_size() const {
        return static_cast<size_t>(width) * height * channels();
    }

    bool is_valid() const {
        return width > 0 && height > 0 && data.size() == expected_size();
    }
};

} // namespace oculus