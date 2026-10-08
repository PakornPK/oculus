#pragma once

#include "oculus/core/frame.hpp"
#include "oculus/core/pose.hpp"
#include "oculus/rom/shoulder_rom_analyzer.hpp"
#include <string>
#include <vector>
#include <functional>

namespace oculus {

enum class AnalysisMode {
    REALTIME,
    RECORD,
    REPLAY,
    IMPORT
};

struct SessionData {
    std::string id;
    std::string name;
    AnalysisMode mode;
    int64_t start_time = 0;
    int64_t end_time = 0;
    std::vector<PoseResult> poses;
    ShoulderRomResult rom_result;
};

class SessionManager {
public:
    void start_session(const std::string& session_name, AnalysisMode mode);
    void stop_session();
    void record_frame(const PoseResult& pose);

    SessionData get_current_session() const;
    std::vector<SessionData> list_sessions() const;
    bool is_recording() const;
    bool is_active() const;

    void export_csv(const std::string& path) const;
    void export_json(const std::string& path) const;

private:
    SessionData current_;
    std::vector<SessionData> history_;
    bool active_ = false;
    bool recording_ = false;
};

} // namespace oculus