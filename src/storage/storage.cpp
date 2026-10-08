#include "oculus/storage/storage.hpp"
#include <spdlog/spdlog.h>

namespace oculus {

Storage::Storage(const StorageConfig& config) : config_(config) {}
Storage::~Storage() { close(); }

bool Storage::open() {
    spdlog::info("Storage opening: {}", config_.path);
    open_ = true;
    return true;
}

void Storage::close() {
    if (open_) {
        spdlog::info("Storage closed");
        open_ = false;
    }
}

bool Storage::is_open() const { return open_; }

bool Storage::save_config(const std::string& key, const std::string& value) {
    if (!open_) return false;
    spdlog::debug("Save config: {}={}", key, value);
    return true;
}

std::string Storage::load_config(const std::string& key, const std::string& default_value) {
    if (!open_) return default_value;
    return default_value;
}

} // namespace oculus