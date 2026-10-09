#pragma once

#include "oculus/core/frame.hpp"
#include "oculus/core/pose.hpp"
#include <memory>
#include <string>
#include <vector>

namespace oculus {

struct PersonBox {
    float x1, y1, x2, y2;
    float confidence;
};

class PersonDetector {
public:
    PersonDetector();
    ~PersonDetector();

    bool load_model(const std::string& model_path);
    std::vector<PersonBox> detect(const Frame& frame, float confidence_threshold = 0.5f);
    bool is_loaded() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace oculus