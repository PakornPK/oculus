#include "oculus/rom/session_manager.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <chrono>

namespace oculus {

void SessionManager::start_session(const std::string& session_name, AnalysisMode mode) {
    if (active_) stop_session();

    current_.id = "session_" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    current_.name = session_name;
    current_.mode = mode;
    current_.start_time = std::chrono::steady_clock::now().time_since_epoch().count();
    current_.poses.clear();
    current_.rom_result = {};

    active_ = true;
    recording_ = (mode == AnalysisMode::RECORD || mode == AnalysisMode::REALTIME);

    spdlog::info("Session started: {} (mode={})", session_name, static_cast<int>(mode));
}

void SessionManager::stop_session() {
    if (!active_) return;

    current_.end_time = std::chrono::steady_clock::now().time_since_epoch().count();
    history_.push_back(current_);
    active_ = false;
    recording_ = false;

    spdlog::info("Session stopped: {} ({} poses)",
                 current_.name, current_.poses.size());
}

void SessionManager::record_frame(const PoseResult& pose) {
    if (!active_ || !recording_) return;
    current_.poses.push_back(pose);
}

SessionData SessionManager::get_current_session() const {
    return current_;
}

std::vector<SessionData> SessionManager::list_sessions() const {
    return history_;
}

bool SessionManager::is_recording() const {
    return recording_;
}

bool SessionManager::is_active() const {
    return active_;
}

void SessionManager::export_csv(const std::string& path) const {
    std::ofstream file(path);
    file << "frame,timestamp,num_poses\n";
    for (size_t i = 0; i < current_.poses.size(); ++i) {
        file << i << "," << current_.poses[i].timestamp
             << "," << current_.poses[i].poses.size() << "\n";
    }
    spdlog::info("Exported CSV: {}", path);
}

void SessionManager::export_json(const std::string& path) const {
    std::ofstream file(path);
    file << "{\n  \"session\": \"" << current_.name << "\",\n";
    file << "  \"frames\": " << current_.poses.size() << "\n}\n";
    spdlog::info("Exported JSON: {}", path);
}

} // namespace oculus