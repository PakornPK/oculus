#include "oculus/inference/models/rtmpose/rtmpose_preprocessor.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <stdexcept>

namespace oculus {

std::vector<float> RTMPosePreprocessor::process(const Frame& frame) const {
    if (!frame.is_valid()) {
        throw std::invalid_argument("Invalid frame: data size mismatch");
    }

    cv::Mat rgb(frame.height, frame.width, CV_8UC3,
                const_cast<uint8_t*>(frame.data.data()));

    // Person detection: center crop with aspect ratio matching model input
    // Model expects 192x256 (WxH), aspect ratio = 192/256 = 0.75
    float model_aspect = static_cast<float>(INPUT_WIDTH) / INPUT_HEIGHT;  // 0.75
    float frame_aspect = static_cast<float>(frame.width) / frame.height;

    int crop_w, crop_h, crop_x, crop_y;

    if (frame_aspect > model_aspect) {
        // Frame is wider than model - crop horizontally
        crop_h = frame.height;
        crop_w = static_cast<int>(crop_h * model_aspect);
        crop_x = (frame.width - crop_w) / 2;
        crop_y = 0;
    } else {
        // Frame is taller than model - crop vertically
        crop_w = frame.width;
        crop_h = static_cast<int>(crop_w / model_aspect);
        crop_x = 0;
        crop_y = (frame.height - crop_h) / 2;
    }

    // Clamp to frame bounds
    crop_x = std::max(0, crop_x);
    crop_y = std::max(0, crop_y);
    crop_w = std::min(crop_w, frame.width - crop_x);
    crop_h = std::min(crop_h, frame.height - crop_y);

    cv::Mat cropped = rgb(cv::Rect(crop_x, crop_y, crop_w, crop_h));

    // Resize crop to model input size
    cv::Mat resized;
    cv::resize(cropped, resized, cv::Size(INPUT_WIDTH, INPUT_HEIGHT),
               0, 0, cv::INTER_LINEAR);

    // Normalize to [0,1] float and convert HWC -> CHW
    std::vector<float> tensor(tensor_size());
    const int hw = INPUT_HEIGHT * INPUT_WIDTH;

    for (int c = 0; c < INPUT_CHANNELS; ++c) {
        for (int h = 0; h < INPUT_HEIGHT; ++h) {
            for (int w = 0; w < INPUT_WIDTH; ++w) {
                uint8_t pixel = resized.at<cv::Vec3b>(h, w)[c];
                tensor[c * hw + h * INPUT_WIDTH + w] = pixel / 255.0f;
            }
        }
    }

    return tensor;
}

} // namespace oculus