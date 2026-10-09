#include "oculus/rom/recipe.hpp"
#include <spdlog/spdlog.h>
#include <fstream>

namespace oculus {

std::string Threshold::classify(float value) const {
    if (value >= normal_min && value <= normal_max) return "Normal";
    if (value >= mild_min && value <= mild_max) return "Mild";
    if (value >= moderate_min && value <= moderate_max) return "Moderate";
    return "Severe";
}

Recipe Recipe::default_shoulder_rom() {
    Recipe recipe;
    recipe.name = "Standard Shoulder ROM";
    recipe.description = "Standard shoulder ROM assessment protocol";
    recipe.side = "both";
    recipe.reps_per_movement = 3;

    auto add_movement = [&](const std::string& name, float normal, float mild, float moderate) {
        MovementConfig mc;
        mc.name = name;
        mc.side = "both";
        mc.threshold.normal_min = normal;
        mc.threshold.normal_max = 180.0f;
        mc.threshold.mild_min = mild;
        mc.threshold.mild_max = normal;
        mc.threshold.moderate_min = moderate;
        mc.threshold.moderate_max = mild;
        recipe.movements.push_back(mc);
    };

    add_movement("forward_flexion", 150.0f, 120.0f, 90.0f);
    add_movement("abduction", 150.0f, 120.0f, 90.0f);
    add_movement("extension", 40.0f, 30.0f, 20.0f);
    add_movement("external_rotation", 80.0f, 60.0f, 40.0f);
    add_movement("internal_rotation", 70.0f, 50.0f, 30.0f);
    add_movement("adduction", 30.0f, 20.0f, 10.0f);
    add_movement("horizontal_adduction", 130.0f, 100.0f, 70.0f);

    recipe.anchoring.reference_landmark = "shoulder_center";
    recipe.anchoring.compensate_rotation = true;
    recipe.anchoring.compensate_position = true;

    return recipe;
}

Recipe Recipe::load(const std::string& path) {
    spdlog::info("Loading recipe from: {}", path);
    return default_shoulder_rom();
}

bool Recipe::save(const std::string& path) const {
    spdlog::info("Saving recipe to: {}", path);
    return true;
}

} // namespace oculus