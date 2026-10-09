#include "oculus/core/hardware_detector.hpp"
#include "oculus/core/config.hpp"
#include "oculus/camera/file_camera.hpp"
#include "oculus/camera/camera_factory.hpp"
#include "oculus/inference/onnx_inference_engine.hpp"
#include "oculus/rom/shoulder_rom_analyzer.hpp"
#include "oculus/rom/session_manager.hpp"
#include "oculus/rom/calibrator.hpp"
#include "oculus/http/web_server.hpp"
#include "oculus/runtime/watchdog.hpp"
#include "oculus/runtime/metrics.hpp"
#include <spdlog/spdlog.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <csignal>

using namespace oculus;

static std::atomic<bool> g_running{true};

void signal_handler(int) { g_running = false; }

void print_usage() {
    std::cout << "Oculus v0.1.0 - Shoulder ROM Analysis\n"
              << "Usage: oculus [options]\n"
              << "  --video <path>    Run demo with video file\n"
              << "  --port <port>     Web server port (default: 8080)\n"
              << "  --diagnose        Run hardware diagnostics\n"
              << "  --help            Show this help\n";
}

int run_demo(const std::string& video_path, int port) {
    spdlog::info("=== Oculus Demo Mode ===");
    spdlog::info("Video: {}", video_path);
    spdlog::info("Port: {}", port);

    auto hw = HardwareDetector::detect();
    spdlog::info("Platform: {} {}", hw.arch, hw.cpu_model);

    FileCamera camera(video_path);
    camera.set_loop(true);

    if (!camera.open()) {
        spdlog::error("Failed to open video: {}", video_path);
        return 1;
    }

    auto cam_info = camera.info();
    spdlog::info("Camera: {} ({}x{})", cam_info.name, cam_info.max_width, cam_info.max_height);

    WebConfig web_config;
    web_config.api_port = port;
    WebServer server(web_config);
    server.start();

    ShoulderRomAnalyzer rom_analyzer;
    Calibrator calibrator;
    SessionManager session_mgr;
    MetricsCollector metrics;

    session_mgr.start_session("demo", AnalysisMode::REALTIME);
    bool calibrated = false;

    spdlog::info("=== Demo Running ===");
    spdlog::info("Open http://localhost:{}/live.html", port);

    int frame_count = 0;
    while (g_running) {
        auto start = std::chrono::steady_clock::now();

        try {
            Frame frame = camera.capture();

            if (!calibrated) {
                Pose dummy_pose;
                dummy_pose.keypoints.resize(17);
                dummy_pose.keypoints[5] = {320, 200, 0.9f};
                dummy_pose.keypoints[6] = {320, 200, 0.9f};
                calibrator.auto_calibrate(dummy_pose);
                calibrated = true;
                spdlog::info("Auto-calibrated");
            }

            Pose simulated_pose;
            simulated_pose.keypoints.resize(17);
            float t = static_cast<float>(frame_count) * 0.05f;

            simulated_pose.keypoints[5] = {280, 200, 0.9f};
            simulated_pose.keypoints[6] = {360, 200, 0.9f};
            simulated_pose.keypoints[7] = {280 + std::sin(t) * 80, 200 + std::cos(t) * 60, 0.85f};
            simulated_pose.keypoints[8] = {360 - std::sin(t) * 80, 200 + std::cos(t) * 60, 0.85f};
            simulated_pose.keypoints[9] = {280 + std::sin(t) * 100, 200 + std::cos(t) * 80, 0.8f};
            simulated_pose.keypoints[10] = {360 - std::sin(t) * 100, 200 + std::cos(t) * 80, 0.8f};
            simulated_pose.keypoints[11] = {280, 350, 0.9f};
            simulated_pose.keypoints[12] = {360, 350, 0.9f};
            simulated_pose.keypoints[3] = {260, 180, 0.8f};
            simulated_pose.keypoints[4] = {380, 180, 0.8f};

            rom_analyzer.update(simulated_pose);

            PoseResult pose_result;
            pose_result.poses.push_back(simulated_pose);
            pose_result.timestamp = frame.timestamp;
            session_mgr.record_frame(pose_result);

            server.increment_frame_count();

            auto left_flex = rom_analyzer.get_rom(MovementType::FORWARD_FLEXION, Side::LEFT);
            auto right_flex = rom_analyzer.get_rom(MovementType::FORWARD_FLEXION, Side::RIGHT);
            auto left_abd = rom_analyzer.get_rom(MovementType::ABDUCTION, Side::LEFT);
            auto right_abd = rom_analyzer.get_rom(MovementType::ABDUCTION, Side::RIGHT);

            server.update_angles({
                {"left_flexion", left_flex.max_angle},
                {"right_flexion", right_flex.max_angle},
                {"left_abduction", left_abd.max_angle},
                {"right_abduction", right_abd.max_angle}
            });

            auto rom_result = rom_analyzer.get_result();
            server.update_result(rom_result);

            if (frame_count % 30 == 0) {
                spdlog::info("Frame {}: L-flex={:.0f}° R-flex={:.0f}° L-abd={:.0f}° R-abd={:.0f}°",
                    frame_count,
                    left_flex.max_angle, right_flex.max_angle,
                    left_abd.max_angle, right_abd.max_angle);
            }

            frame_count++;

        } catch (const std::exception& e) {
            spdlog::error("Frame error: {}", e.what());
        }

        auto end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        metrics.record_latency("frame", ms);

        if (frame_count % 100 == 0) {
            spdlog::info("FPS: {:.1}, avg latency: {:.1}ms",
                1000.0 / metrics.get_avg_latency("frame"),
                metrics.get_avg_latency("frame"));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }

    session_mgr.stop_session();
    server.stop();

    spdlog::info("=== Demo Complete ===");
    spdlog::info("Frames processed: {}", frame_count);
    spdlog::info("Avg latency: {:.1}ms", metrics.get_avg_latency("frame"));

    return 0;
}

int run_diagnose() {
    spdlog::info("=== Oculus Diagnostics ===");
    auto results = HardwareDetector::diagnose();

    int pass = 0, fail = 0;
    for (const auto& r : results) {
        if (r.ok) pass++;
        else fail++;
    }

    spdlog::info("Results: {} pass, {} fail", pass, fail);
    return fail > 0 ? 1 : 0;
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::string video_path;
    int port = 8080;
    bool diagnose = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--video" && i + 1 < argc) {
            video_path = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--diagnose") {
            diagnose = true;
        } else if (arg == "--help") {
            print_usage();
            return 0;
        }
    }

    if (diagnose) return run_diagnose();
    if (!video_path.empty()) return run_demo(video_path, port);

    print_usage();
    return 0;
}