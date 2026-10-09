#pragma once

#include "oculus/core/frame.hpp"
#include <opencv2/core.hpp>
#include <vector>

namespace oculus {

class RTMPosePreprocessor {
public:
    static constexpr int INPUT_WIDTH = 192;
    static constexpr int INPUT_HEIGHT = 256;
    static constexpr int INPUT_CHANNELS = 3;

    std::vector<float> process(const Frame& frame) const;

    std::vector<float> process_with_bbox(const Frame& frame,
        float bbox_x1, float bbox_y1,
        float bbox_x2, float bbox_y2) const;

    int input_width() const { return INPUT_WIDTH; }
    int input_height() const { return INPUT_HEIGHT; }
    int input_channels() const { return INPUT_CHANNELS; }
    size_t tensor_size() const {
        return static_cast<size_t>(INPUT_CHANNELS) * INPUT_HEIGHT * INPUT_WIDTH;
    }

private:
    std::vector<float> process_crop(const cv::Mat& rgb,
        int crop_x, int crop_y, int crop_w, int crop_h) const;
};

} // namespace oculus