#pragma once

#include <memory>
#include <string>
#include <functional>

namespace oculus {

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

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace oculus