#include "oculus/inference/models/rtmpose/rtmpose_preprocessor.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace oculus {

// ImageNet normalization constants
static constexpr float MEAN[] = {0.485f, 0.446f, 0.406f};
static constexpr float STD[] = {0.229f, 0.224f, 0.225f};

std::vector<float> RTMPosePreprocessor::process(const Frame& frame) const {
    if (!frame.is_valid()) {
        throw std::invalid_argument("Invalid frame: data size mismatch");
    }

    cv::Mat rgb(frame.height, frame.width, CV_8UC3,
                const_cast<uint8_t*>(frame.data.data()));

    // Default: center crop with model aspect ratio
    // This will be overridden when person detection provides a bbox
    float model_aspect = static_cast<float>(INPUT_WIDTH) / INPUT_HEIGHT;
    float frame_aspect = static_cast<float>(frame.width) / frame.height;

    int crop_x = 0, crop_y = 0, crop_w = frame.width, crop_h = frame.height;

    if (frame_aspect > model_aspect) {
        crop_h = frame.height;
        crop_w = static_cast<int>(crop_h * model_aspect);
        crop_x = (frame.width - crop_w) / 2;
        crop_y = 0;
    } else {
        crop_w = frame.width;
        crop_h = static_cast<int>(crop_w / model_aspect);
        crop_x = 0;
        crop_y = (frame.height - crop_h) / 2;
    }

    crop_x = std::max(0, crop_x);
    crop_y = std::max(0, crop_y);
    crop_w = std::min(crop_w, frame.width - crop_x);
    crop_h = std::min(crop_h, frame.height - crop_y);

    return process_crop(rgb, crop_x, crop_y, crop_w, crop_h);
}

std::vector<float> RTMPosePreprocessor::process_with_bbox(
    const Frame& frame, float bbox_x1, float bbox_y1,
    float bbox_x2, float bbox_y2) const {

    if (!frame.is_valid()) {
        throw std::invalid_argument("Invalid frame: data size mismatch");
    }

    cv::Mat rgb(frame.height, frame.width, CV_8UC3,
                const_cast<uint8_t*>(frame.data.data()));

    // Expand bbox by 20% for better context
    float bbox_w = bbox_x2 - bbox_x1;
    float bbox_h = bbox_y2 - bbox_y1;
    float expand_x = bbox_w * 0.2f;
    float expand_y = bbox_h * 0.2f;

    int crop_x = static_cast<int>(std::max(0.0f, bbox_x1 - expand_x));
    int crop_y = static_cast<int>(std::max(0.0f, bbox_y1 - expand_y));
    int crop_w = static_cast<int>(std::min(
        static_cast<float>(frame.width), bbox_x2 + expand_x) - crop_x);
    int crop_h = static_cast<int>(std::min(
        static_cast<float>(frame.height), bbox_y2 + expand_y) - crop_y);

    // Adjust to model aspect ratio
    float model_aspect = static_cast<float>(INPUT_WIDTH) / INPUT_HEIGHT;
    float crop_aspect = static_cast<float>(crop_w) / crop_h;

    if (crop_aspect > model_aspect) {
        int new_h = static_cast<int>(crop_w / model_aspect);
        crop_y -= (new_h - crop_h) / 2;
        crop_h = new_h;
    } else {
        int new_w = static_cast<int>(crop_h * model_aspect);
        crop_x -= (new_w - crop_w) / 2;
        crop_w = new_w;
    }

    // Clamp to frame bounds
    crop_x = std::max(0, crop_x);
    crop_y = std::max(0, crop_y);
    crop_w = std::min(crop_w, frame.width - crop_x);
    crop_h = std::min(crop_h, frame.height - crop_y);

    return process_crop(rgb, crop_x, crop_y, crop_w, crop_h);
}

std::vector<float> RTMPosePreprocessor::process_crop(
    const cv::Mat& rgb, int crop_x, int crop_y, int crop_w, int crop_h) const {

    cv::Mat cropped = rgb(cv::Rect(crop_x, crop_y, crop_w, crop_h));

    // CLAHE histogram equalization for better contrast
    cv::Mat lab;
    cv::cvtColor(cropped, lab, cv::COLOR_RGB2Lab);
    std::vector<cv::Mat> lab_channels;
    cv::split(lab, lab_channels);
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
    clahe->apply(lab_channels[0], lab_channels[0]);
    cv::merge(lab_channels, lab);
    cv::Mat enhanced;
    cv::cvtColor(lab, enhanced, cv::COLOR_Lab2RGB);

    // Resize to model input
    cv::Mat resized;
    cv::resize(enhanced, resized, cv::Size(INPUT_WIDTH, INPUT_HEIGHT),
               0, 0, cv::INTER_LINEAR);

    // Normalize with ImageNet mean/std and convert HWC -> CHW
    std::vector<float> tensor(tensor_size());
    const int hw = INPUT_HEIGHT * INPUT_WIDTH;

    for (int c = 0; c < INPUT_CHANNELS; ++c) {
        for (int h = 0; h < INPUT_HEIGHT; ++h) {
            for (int w = 0; w < INPUT_WIDTH; ++w) {
                float pixel = resized.at<cv::Vec3b>(h, w)[c] / 255.0f;
                tensor[c * hw + h * INPUT_WIDTH + w] =
                    (pixel - MEAN[c]) / STD[c];
            }
        }
    }

    return tensor;
}

} // namespace oculus