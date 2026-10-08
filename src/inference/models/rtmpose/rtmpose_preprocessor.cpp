#include "oculus/inference/models/rtmpose/rtmpose_preprocessor.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>

namespace oculus {

std::vector<float> RTMPosePreprocessor::process(const Frame& frame) const {
    if (!frame.is_valid()) {
        throw std::invalid_argument("Invalid frame: data size mismatch");
    }

    // Convert raw data to OpenCV Mat (RGB, HWC)
    cv::Mat rgb(frame.height, frame.width, CV_8UC3,
                const_cast<uint8_t*>(frame.data.data()));

    // Convert RGB to BGR for OpenCV (cv::resize works on either, but
    // standard OpenCV convention is BGR; RTMPose expects RGB input so we
    // stay in RGB and just resize).
    cv::Mat resized;
    cv::resize(rgb, resized, cv::Size(INPUT_WIDTH, INPUT_HEIGHT),
               0, 0, cv::INTER_LINEAR);

    // Normalize to [0,1] float and convert HWC -> CHW
    std::vector<float> tensor(tensor_size());
    const int hw = INPUT_HEIGHT * INPUT_WIDTH;

    for (int c = 0; c < INPUT_CHANNELS; ++c) {
        for (int h = 0; h < INPUT_HEIGHT; ++h) {
            for (int w = 0; w < INPUT_WIDTH; ++w) {
                // OpenCV Mat is HWC, channels in BGR order but we store RGB as-is
                uint8_t pixel = resized.at<cv::Vec3b>(h, w)[c];
                tensor[c * hw + h * INPUT_WIDTH + w] = pixel / 255.0f;
            }
        }
    }

    return tensor;
}

} // namespace oculus