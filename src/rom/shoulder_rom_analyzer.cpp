#include "oculus/rom/shoulder_rom_analyzer.hpp"
#include <cmath>

namespace oculus {

std::string ShoulderRomAnalyzer::movement_name(MovementType movement) {
    switch (movement) {
        case MovementType::ABDUCTION: return "abduction";
        case MovementType::FORWARD_FLEXION: return "forward_flexion";
        case MovementType::EXTENSION: return "extension";
        case MovementType::EXTERNAL_ROTATION: return "external_rotation";
        case MovementType::INTERNAL_ROTATION: return "internal_rotation";
        case MovementType::ADDUCTION: return "adduction";
        case MovementType::HORIZONTAL_ADDDUCTION: return "horizontal_adduction";
        case MovementType::SCAPULAR_PROTRACTION: return "scapular_protraction";
        case MovementType::SCAPULAR_RETRACTION: return "scapular_retraction";
        case MovementType::SHOULDER_ELEVATION: return "shoulder_elevation";
        case MovementType::SHOULDER_DEPRESSION: return "shoulder_depression";
    }
    return "unknown";
}

void ShoulderRomAnalyzer::update(const Pose& pose) {
    std::vector<MovementType> movements = {
        MovementType::ABDUCTION,
        MovementType::FORWARD_FLEXION,
        MovementType::EXTENSION,
        MovementType::EXTERNAL_ROTATION,
        MovementType::INTERNAL_ROTATION,
        MovementType::ADDUCTION,
        MovementType::HORIZONTAL_ADDDUCTION,
        MovementType::SCAPULAR_PROTRACTION,
        MovementType::SCAPULAR_RETRACTION,
        MovementType::SHOULDER_ELEVATION,
        MovementType::SHOULDER_DEPRESSION
    };

    for (auto movement : movements) {
        for (auto side : {Side::LEFT, Side::RIGHT}) {
            Key key{movement, side};

            float raw_angle = calculator_.calculate_angle(pose, movement, side);

            auto& smoother = smoothers_[key];
            float smoothed = smoother.smooth(raw_angle);

            rom_data_[key].update(smoothed);
        }
    }
}

void ShoulderRomAnalyzer::reset() {
    rom_data_.clear();
    smoothers_.clear();
}

ShoulderRomResult ShoulderRomAnalyzer::get_result() const {
    ShoulderRomResult result;

    for (const auto& [key, data] : rom_data_) {
        auto name = movement_name(key.first);
        if (key.second == Side::LEFT) {
            result.left[name] = data;
        } else {
            result.right[name] = data;
        }
    }

    float left_total = 0.0f;
    float right_total = 0.0f;
    int count = 0;

    for (const auto& [name, left_data] : result.left) {
        auto it = result.right.find(name);
        if (it != result.right.end() && left_data.rom > 0 && it->second.rom > 0) {
            left_total += left_data.rom;
            right_total += it->second.rom;
            count++;
        }
    }

    if (count > 0 && left_total > 0 && right_total > 0) {
        float avg = (left_total + right_total) / 2.0f;
        result.symmetry_pct = (std::min(left_total, right_total) / avg) * 100.0f;
    }

    return result;
}

float ShoulderRomAnalyzer::get_current_angle(MovementType movement, Side side) const {
    Key key{movement, side};
    auto it = smoothers_.find(key);
    if (it != smoothers_.end()) {
        return it->second.average();
    }
    return 0.0f;
}

RomData ShoulderRomAnalyzer::get_rom(MovementType movement, Side side) const {
    Key key{movement, side};
    auto it = rom_data_.find(key);
    if (it != rom_data_.end()) {
        return it->second;
    }
    return RomData{};
}

} // namespace oculus