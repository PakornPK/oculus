#pragma once

#include "oculus/rom/joint_angle_calculator.hpp"
#include "oculus/rom/angle_smoother.hpp"
#include <string>
#include <unordered_map>

namespace oculus {

struct RomData {
    float min_angle = 999.0f;
    float max_angle = -999.0f;
    float rom = 0.0f;
    int sample_count = 0;

    void update(float angle) {
        if (angle < min_angle) min_angle = angle;
        if (angle > max_angle) max_angle = angle;
        rom = max_angle - min_angle;
        sample_count++;
    }

    void reset() {
        min_angle = 999.0f;
        max_angle = -999.0f;
        rom = 0.0f;
        sample_count = 0;
    }
};

struct ShoulderRomResult {
    std::unordered_map<std::string, RomData> left;
    std::unordered_map<std::string, RomData> right;
    float symmetry_pct = 0.0f;
};

class ShoulderRomAnalyzer {
public:
    void update(const Pose& pose);
    void reset();

    ShoulderRomResult get_result() const;
    float get_current_angle(MovementType movement, Side side) const;
    RomData get_rom(MovementType movement, Side side) const;

    static std::string movement_name(MovementType movement);

private:
    JointAngleCalculator calculator_;

    using Key = std::pair<MovementType, Side>;
    struct KeyHash {
        size_t operator()(const Key& k) const {
            return std::hash<int>()(static_cast<int>(k.first)) ^
                   (std::hash<int>()(static_cast<int>(k.second)) << 1);
        }
    };

    std::unordered_map<Key, RomData, KeyHash> rom_data_;
    std::unordered_map<Key, AngleSmoother, KeyHash> smoothers_;
};

} // namespace oculus