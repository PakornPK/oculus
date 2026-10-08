#pragma once

#include <string>

namespace oculus {

struct AppConfig {
    std::string hostname = "oculus";
    int http_port = 80;
    int api_port = 8080;
    bool mdns_enabled = true;

    struct {
        std::string device = "auto";
        int width = 640;
        int height = 480;
        int fps = 30;
    } camera;

    struct {
        std::string model_path = "models/rtmpose.onnx";
        std::string backend = "auto";
        std::string precision = "fp16";
        int num_threads = 4;
    } inference;

    struct {
        std::string mode = "realtime";
        std::string analysis_type = "active_rom";
        int reps_per_movement = 3;
    } rom;

    static AppConfig load(const std::string& path);
    static AppConfig defaults();
    bool save(const std::string& path) const;
};

} // namespace oculus