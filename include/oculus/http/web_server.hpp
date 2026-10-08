#pragma once

#include <memory>
#include <string>
#include <functional>
#include <unordered_map>

namespace oculus {

struct ShoulderRomResult;

struct WebConfig {
    int port = 80;
    int api_port = 8080;
    std::string static_root = "web/";
};

class WebServer {
public:
    explicit WebServer(const WebConfig& config);
    ~WebServer();

    void start();
    void stop();
    bool is_running() const;

    // Push real-time data to connected SSE clients
    void update_angles(const std::unordered_map<std::string, float>& angles);
    void update_result(const ShoulderRomResult& result);
    void increment_frame_count();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace oculus