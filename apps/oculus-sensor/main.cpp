#include "oculus/core/hardware_detector.hpp"
#include "oculus/core/config.hpp"
#include "oculus/camera/camera_factory.hpp"
#include "oculus/inference/onnx_inference_engine.hpp"
#include "oculus/rom/shoulder_rom_analyzer.hpp"
#include "oculus/rom/session_manager.hpp"
#include "oculus/http/web_server.hpp"
#include "oculus/runtime/watchdog.hpp"
#include "oculus/runtime/metrics.hpp"
#include <spdlog/spdlog.h>

using namespace oculus;

int main() {
    spdlog::info("=== Oculus v0.1.0 ===");

    auto hw = HardwareDetector::detect();
    spdlog::info("Platform: {} {}", hw.arch, hw.cpu_model);
    spdlog::info("Backend: {} (gpu={})", hw.selected_backend, hw.gpu_accelerated);

    auto diag = HardwareDetector::diagnose();
    for (const auto& d : diag) {
        spdlog::info("[DIAG] {}: {} - {}", d.component,
                     d.ok ? "OK" : "FAIL", d.message);
    }

    AppConfig config = AppConfig::defaults();
    spdlog::info("Config loaded: hostname={}", config.hostname);

    WebConfig web_config;
    web_config.api_port = config.api_port;
    WebServer server(web_config);

    Watchdog watchdog;
    watchdog.start(std::chrono::seconds(30));

    MetricsCollector metrics;

    spdlog::info("=== Oculus Ready ===");
    spdlog::info("API: http://{}:{}", config.hostname, config.api_port);

    server.start();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    server.stop();
    watchdog.stop();

    spdlog::info("=== Oculus Shutdown ===");
    return 0;
}