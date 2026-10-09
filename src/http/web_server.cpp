#include "oculus/http/web_server.hpp"
#include "oculus/rom/shoulder_rom_analyzer.hpp"
#include "oculus/rom/session_manager.hpp"
#include "oculus/core/hardware_detector.hpp"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <sstream>
#include <set>

using json = nlohmann::json;

namespace oculus {

struct WebServer::Impl {
    WebConfig config;
    std::unique_ptr<httplib::Server> server;
    std::thread server_thread;
    std::atomic<bool> running{false};

    // Real-time state (set by the application layer)
    std::mutex state_mutex;
    ShoulderRomResult current_result;
    std::unordered_map<std::string, float> current_angles;  // movement_side -> angle
    int64_t start_time = 0;
    int total_frames = 0;
    int total_sessions = 0;

    // SSE clients
    std::mutex sse_mutex;
    std::set<httplib::DataSink*> sse_clients;

    // MJPEG stream
    std::mutex frame_mutex;
    std::vector<uint8_t> latest_frame;
    std::set<httplib::DataSink*> mjpeg_clients;

    void register_routes();
    json rom_result_to_json(const ShoulderRomResult& result);
    json session_to_json(const SessionData& session);
    void broadcast_sse(const std::string& data);
};

WebServer::WebServer(const WebConfig& config)
    : impl_(std::make_unique<Impl>()) {
    impl_->config = config;
    impl_->server = std::make_unique<httplib::Server>();
    impl_->start_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    impl_->register_routes();
}

WebServer::~WebServer() {
    stop();
}

void WebServer::Impl::register_routes() {
    // ── Static files ──
    if (!config.static_root.empty()) {
        server->set_mount_point("/", config.static_root);
        spdlog::info("Serving static files from {}", config.static_root);
    }

    // ── Health ──
    server->Get("/api/v1/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"status":"ok"})", "application/json");
    });

    // ── System info ──
    server->Get("/api/v1/system/info", [](const httplib::Request&, httplib::Response& res) {
        json info;
        info["name"] = "Oculus";
        info["version"] = "0.1.0";
        try {
            auto hw = HardwareDetector::detect();
            info["arch"] = hw.arch;
            info["os"] = hw.os;
            info["hostname"] = hw.hostname;
            info["cpu_model"] = hw.cpu_model;
            info["cpu_cores"] = hw.cpu_cores;
            info["has_nvidia_gpu"] = hw.has_nvidia_gpu;
            info["gpu_model"] = hw.gpu_model;
            info["gpu_memory_mb"] = hw.gpu_memory_mb;
            info["total_memory_mb"] = hw.total_memory_mb;
            info["backend"] = hw.selected_backend;
            info["gpu_accelerated"] = hw.gpu_accelerated;
        } catch (...) {
            info["error"] = "hardware detection failed";
        }
        res.set_content(info.dump(), "application/json");
    });

    // ── Current ROM angles ──
    server->Get("/api/v1/rom/angles", [this](const httplib::Request&, httplib::Response& res) {
        json data;
        json angles;
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            for (const auto& [key, angle] : current_angles) {
                // key format: "movement_side"
                auto pos = key.rfind('_');
                if (pos != std::string::npos) {
                    std::string movement = key.substr(0, pos);
                    std::string side = key.substr(pos + 1);
                    if (!angles.contains(movement)) angles[movement] = json::object();
                    angles[movement][side] = angle;
                }
            }
        }
        data["angles"] = angles;
        data["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        res.set_content(data.dump(), "application/json");
    });

    // ── ROM metrics ──
    server->Get("/api/v1/rom/metrics", [this](const httplib::Request&, httplib::Response& res) {
        json data;
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            data["uptime_seconds"] = now - start_time;
            data["total_frames"] = total_frames;
            data["total_sessions"] = total_sessions;
        }
        res.set_content(data.dump(), "application/json");
    });

    // ── ROM analysis results ──
    server->Get("/api/v1/rom/analysis", [this](const httplib::Request&, httplib::Response& res) {
        json data;
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            data = rom_result_to_json(current_result);
        }
        res.set_content(data.dump(), "application/json");
    });

    // ── List sessions ──
    server->Get("/api/v1/sessions", [this](const httplib::Request&, httplib::Response& res) {
        json sessions = json::array();
        // Sessions are managed externally; this returns a placeholder
        // that the application layer can populate via update_sessions()
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            // Return count-based placeholder
            json s;
            s["id"] = "current";
            s["name"] = "Current Session";
            s["mode"] = "REALTIME";
            s["start_time"] = start_time;
            s["frames"] = total_frames;
            sessions.push_back(s);
        }
        res.set_content(sessions.dump(), "application/json");
    });

    // ── MJPEG video stream ──
    server->Get("/api/v1/camera/stream", [this](const httplib::Request&, httplib::Response& res) {
        res.set_header("Content-Type", "multipart/x-mixed-replace; boundary=frame");
        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");
        res.set_header("Access-Control-Allow-Origin", "*");

        res.set_chunked_content_provider(
            "multipart/x-mixed-replace; boundary=frame",
            [this](size_t /*offset*/, httplib::DataSink& sink) {
                {
                    std::lock_guard<std::mutex> lock(frame_mutex);
                    mjpeg_clients.insert(&sink);
                }
                while (sink.is_writable()) {
                    std::vector<uint8_t> frame_data;
                    {
                        std::lock_guard<std::mutex> lock(frame_mutex);
                        frame_data = latest_frame;
                    }
                    if (!frame_data.empty()) {
                        std::string header = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " +
                            std::to_string(frame_data.size()) + "\r\n\r\n";
                        sink.write(header.c_str(), header.size());
                        sink.write(reinterpret_cast<const char*>(frame_data.data()), frame_data.size());
                        sink.write("\r\n", 2);
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(33));
                }
                {
                    std::lock_guard<std::mutex> lock(frame_mutex);
                    mjpeg_clients.erase(&sink);
                }
                return true;
            });
    });

    // ── SSE endpoint for real-time ROM data ──
    server->Get("/ws/rom", [this](const httplib::Request&, httplib::Response& res) {
        res.set_header("Content-Type", "text/event-stream");
        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");
        res.set_header("Access-Control-Allow-Origin", "*");

        res.set_chunked_content_provider(
            "text/event-stream",
            [this](size_t /*offset*/, httplib::DataSink& sink) {
                {
                    std::lock_guard<std::mutex> lock(sse_mutex);
                    sse_clients.insert(&sink);
                }
                // Keep the connection alive
                while (sink.is_writable()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    // Send keepalive comment
                    sink.write(": keepalive\n\n", 14);
                }
                {
                    std::lock_guard<std::mutex> lock(sse_mutex);
                    sse_clients.erase(&sink);
                }
                return true;
            });
    });
}

json WebServer::Impl::rom_result_to_json(const ShoulderRomResult& result) {
    json data;
    json left = json::object();
    json right = json::object();

    auto rom_to_json = [](const RomData& rom) -> json {
        json obj;
        if (rom.min_angle < 900) obj["min_angle"] = rom.min_angle;
        else obj["min_angle"] = nullptr;
        if (rom.max_angle > -900) obj["max_angle"] = rom.max_angle;
        else obj["max_angle"] = nullptr;
        obj["rom"] = rom.rom;
        obj["sample_count"] = rom.sample_count;
        return obj;
    };

    for (const auto& [name, rom] : result.left) {
        left[name] = rom_to_json(rom);
    }
    for (const auto& [name, rom] : result.right) {
        right[name] = rom_to_json(rom);
    }

    data["left"] = left;
    data["right"] = right;
    data["symmetry_pct"] = result.symmetry_pct;
    return data;
}

json WebServer::Impl::session_to_json(const SessionData& session) {
    json s;
    s["id"] = session.id;
    s["name"] = session.name;
    switch (session.mode) {
        case AnalysisMode::REALTIME: s["mode"] = "REALTIME"; break;
        case AnalysisMode::RECORD:   s["mode"] = "RECORD"; break;
        case AnalysisMode::REPLAY:   s["mode"] = "REPLAY"; break;
        case AnalysisMode::IMPORT:   s["mode"] = "IMPORT"; break;
    }
    s["start_time"] = session.start_time;
    s["end_time"] = session.end_time;
    s["frame_count"] = session.poses.size();
    return s;
}

void WebServer::Impl::broadcast_sse(const std::string& data) {
    std::string message = "data: " + data + "\n\n";
    std::lock_guard<std::mutex> lock(sse_mutex);
    for (auto* sink : sse_clients) {
        if (sink->is_writable()) {
            sink->write(message.c_str(), message.size());
        }
    }
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

void WebServer::update_angles(const std::unordered_map<std::string, float>& angles) {
    {
        std::lock_guard<std::mutex> lock(impl_->state_mutex);
        impl_->current_angles = angles;
    }

    // Broadcast via SSE
    json data;
    json angle_obj;
    for (const auto& [key, angle] : angles) {
        auto pos = key.rfind('_');
        if (pos != std::string::npos) {
            std::string movement = key.substr(0, pos);
            std::string side = key.substr(pos + 1);
            if (!angle_obj.contains(movement)) angle_obj[movement] = json::object();
            angle_obj[movement][side] = angle;
        }
    }
    data["type"] = "rom_angles";
    data["angles"] = angle_obj;
    data["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    impl_->broadcast_sse(data.dump());
}

void WebServer::update_result(const ShoulderRomResult& result) {
    std::lock_guard<std::mutex> lock(impl_->state_mutex);
    impl_->current_result = result;
}

void WebServer::increment_frame_count() {
    std::lock_guard<std::mutex> lock(impl_->state_mutex);
    impl_->total_frames++;
}

void WebServer::push_frame(const std::vector<uint8_t>& jpeg_data) {
    {
        std::lock_guard<std::mutex> lock(impl_->frame_mutex);
        impl_->latest_frame = jpeg_data;
    }

    // Also broadcast via SSE as base64
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    encoded.reserve(((jpeg_data.size() + 2) / 3) * 4);
    for (size_t i = 0; i < jpeg_data.size(); i += 3) {
        unsigned int n = static_cast<unsigned int>(jpeg_data[i]) << 16;
        if (i + 1 < jpeg_data.size()) n |= static_cast<unsigned int>(jpeg_data[i + 1]) << 8;
        if (i + 2 < jpeg_data.size()) n |= static_cast<unsigned int>(jpeg_data[i + 2]);
        encoded += b64[(n >> 18) & 0x3F];
        encoded += b64[(n >> 12) & 0x3F];
        encoded += (i + 1 < jpeg_data.size()) ? b64[(n >> 6) & 0x3F] : '=';
        encoded += (i + 2 < jpeg_data.size()) ? b64[n & 0x3F] : '=';
    }

    json data;
    data["type"] = "video_frame";
    data["frame"] = encoded;
    impl_->broadcast_sse(data.dump());
}

} // namespace oculus