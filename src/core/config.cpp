#include "oculus/core/config.hpp"
#include <spdlog/spdlog.h>

namespace oculus {

AppConfig AppConfig::defaults() {
    return AppConfig{};
}

AppConfig AppConfig::load(const std::string& path) {
    spdlog::info("Loading config from: {}", path);
    return defaults();
}

bool AppConfig::save(const std::string& path) const {
    spdlog::info("Saving config to: {}", path);
    return true;
}

} // namespace oculus