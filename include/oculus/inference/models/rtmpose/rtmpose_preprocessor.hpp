#pragma once

#include "oculus/core/frame.hpp"
#include <vector>

namespace oculus {

class RTMPosePreprocessor {
public:
    static constexpr int INPUT_WIDTH = 256;
    static constexpr int INPUT_HEIGHT = 192;
    static constexpr int INPUT_CHANNELS = 3;

    // Preprocess a Frame (640x480 RGB) into a normalized float tensor (1, 3, 192, 256).
    // Resize to 256x192, normalize pixel values to [0,1], convert HWC -> CHW.
    std::vector<float> process(const Frame& frame) const;

    int input_width() const { return INPUT_WIDTH; }
    int input_height() const { return INPUT_HEIGHT; }
    int input_channels() const { return INPUT_CHANNELS; }
    size_t tensor_size() const {
        return static_cast<size_t>(INPUT_CHANNELS) * INPUT_HEIGHT * INPUT_WIDTH;
    }
};

} // namespace oculus