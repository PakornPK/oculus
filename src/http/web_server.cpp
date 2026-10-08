#include "oculus/http/web_server.hpp"
#include <httplib.h>
#include <spdlog/spdlog.h>
#include <thread>

namespace oculus {

struct WebServer::Impl {
    WebConfig config;
    std::unique_ptr<httplib::Server> server;
    std::thread server_thread;
    bool running = false;
};

WebServer::WebServer(const WebConfig& config)
    : impl_(std::make_unique<Impl>()) {
    impl_->config = config;
    impl_->server = std::make_unique<httplib::Server>();

    impl_->server->Get("/api/v1/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    impl_->server->Get("/api/v1/system/info", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"version\":\"0.1.0\",\"name\":\"oculus\"}", "application/json");
    });
}

WebServer::~WebServer() {
    stop();
}

void WebServer::start() {
    if (impl_->running) return;

    impl_->server_thread = std::thread([this]() {
        spdlog::info("Web server starting on port {}", impl_->config.api_port);
        impl_->running = true;
        impl_->server->listen("0.0.0.0", impl_->config.api_port);
        impl_->running = false;
    });
}

void WebServer::stop() {
    if (!impl_->running) return;
    impl_->server->stop();
    if (impl_->server_thread.joinable()) {
        impl_->server_thread.join();
    }
}

bool WebServer::is_running() const {
    return impl_->running;
}

} // namespace oculus