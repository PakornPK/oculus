#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace oculus {

struct Threshold {
    float normal_min = 0.0f;
    float normal_max = 180.0f;
    float mild_min = 0.0f;
    float mild_max = 0.0f;
    float moderate_min = 0.0f;
    float moderate_max = 0.0f;

    std::string classify(float value) const;
};

struct MovementConfig {
    std::string name;
    std::string side;  // "left", "right", "both"
    Threshold threshold;
    bool enabled = true;
};

struct AnchoringConfig {
    std::string reference_landmark = "shoulder_center";
    bool compensate_rotation = true;
    bool compensate_position = true;
};

struct Recipe {
    std::string name;
    std::string description;
    std::string side;  // default side
    std::vector<MovementConfig> movements;
    AnchoringConfig anchoring;
    int reps_per_movement = 3;

    static Recipe default_shoulder_rom();
    static Recipe load(const std::string& path);
    bool save(const std::string& path) const;
};

} // namespace oculus