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
#include <fstream>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

using namespace oculus;

static std::atomic<bool> g_running{true};

void signal_handler(int) { g_running = false; }

void print_usage() {
    std::cout << "Oculus v0.1.0 - Shoulder ROM Analysis\n"
              << "Usage: oculus [options]\n"
              << "  --video <path>    Run demo with video file\n"
              << "  --model <path>    ONNX model path (e.g., models/rtmpose-m.onnx)\n"
              << "  --port <port>     Web server port (default: 8080)\n"
              << "  --diagnose        Run hardware diagnostics\n"
              << "  --help            Show this help\n";
}

int run_demo(const std::string& video_path, int port, const std::string& model_path) {
    spdlog::info("=== Oculus Demo Mode ===");
    spdlog::info("Video: {}", video_path);
    spdlog::info("Model: {}", model_path.empty() ? "(simulated)" : model_path);
    spdlog::info("Port: {}", port);

    auto hw = HardwareDetector::detect();
    spdlog::info("Platform: {} {}", hw.arch, hw.cpu_model);

    std::unique_ptr<InferenceEngine> engine;
    if (!model_path.empty()) {
        engine = std::make_unique<OnnxInferenceEngine>();
        if (!engine->load_model(model_path)) {
            spdlog::error("Failed to load model: {}", model_path);
            return 1;
        }
        spdlog::info("Model loaded: {}", engine->backend_name());
    }

    FileCamera camera(video_path);
    camera.set_loop(true);

    camera.set_loop(true);

    if (!camera.open()) {
        spdlog::error("Failed to open video: {}", video_path);
        return 1;
    }

    auto cam_info = camera.info();
    spdlog::info("Camera: {} ({}x{})", cam_info.name, cam_info.max_width, cam_info.max_height);

    WebConfig web_config;
    web_config.api_port = port;

    // Find web/ directory relative to executable or project root
    std::string web_root = "web/";
    std::ifstream test_file("web/index.html");
    if (!test_file.good()) {
        web_root = "../web/";
        std::ifstream test_file2("../web/index.html");
        if (!test_file2.good()) {
            web_root = "../../web/";
        }
    }
    web_config.static_root = web_root;
    spdlog::info("Static files: {}", web_root);

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

            Pose pose;
            if (engine) {
                PoseResult result = engine->infer(frame);
                if (!result.poses.empty()) {
                    pose = result.poses[0];
                } else {
                    pose.keypoints.resize(17);
                }
            } else {
                pose.keypoints.resize(17);
                float t = static_cast<float>(frame_count) * 0.05f;
                pose.keypoints[5] = {280, 200, 0.9f};
                pose.keypoints[6] = {360, 200, 0.9f};
                pose.keypoints[7] = {280 + std::sin(t) * 80, 200 + std::cos(t) * 60, 0.85f};
                pose.keypoints[8] = {360 - std::sin(t) * 80, 200 + std::cos(t) * 60, 0.85f};
                pose.keypoints[9] = {280 + std::sin(t) * 100, 200 + std::cos(t) * 80, 0.8f};
                pose.keypoints[10] = {360 - std::sin(t) * 100, 200 + std::cos(t) * 80, 0.8f};
                pose.keypoints[11] = {280, 350, 0.9f};
                pose.keypoints[12] = {360, 350, 0.9f};
                pose.keypoints[3] = {260, 180, 0.8f};
                pose.keypoints[4] = {380, 180, 0.8f};
            }

            // Scale keypoints from model input (192x256) to video size (640x360)
            float scale_x = static_cast<float>(frame.width) / 192.0f;
            float scale_y = static_cast<float>(frame.height) / 256.0f;
            for (auto& kp : pose.keypoints) {
                kp.x *= scale_x;
                kp.y *= scale_y;
            }

            rom_analyzer.update(pose);

            cv::Mat cv_frame(frame.height, frame.width, CV_8UC3, frame.data.data());
            cv::Mat bgr_frame;
            cv::cvtColor(cv_frame, bgr_frame, cv::COLOR_RGB2BGR);

            // Draw full skeleton (17 keypoints + bones)
            const int skeleton[][2] = {
                {0,1},{0,2},{1,3},{2,4},           // face
                {5,6},                               // shoulders
                {5,7},{7,9},                         // left arm
                {6,8},{8,10},                        // right arm
                {5,11},{6,12},                       // torso
                {11,12},                             // hips
                {11,13},{13,15},                     // left leg
                {12,14},{14,16}                      // right leg
            };

            // Draw bones
            for (const auto& bone : skeleton) {
                int i = bone[0], j = bone[1];
                if (i < pose.keypoints.size() && j < pose.keypoints.size()) {
                    auto& a = pose.keypoints[i];
                    auto& b = pose.keypoints[j];
                    if (a.confidence > 0.3f && b.confidence > 0.3f) {
                        cv::line(bgr_frame, cv::Point(a.x, a.y), cv::Point(b.x, b.y),
                                 cv::Scalar(0, 255, 255), 2);
                    }
                }
            }

            // Draw keypoints
            for (int i = 0; i < pose.keypoints.size(); ++i) {
                auto& kp = pose.keypoints[i];
                if (kp.confidence > 0.3f) {
                    cv::Scalar color = (i == 5 || i == 7 || i == 9 || i == 11 || i == 13 || i == 15)
                        ? cv::Scalar(0, 255, 0)   // left = green
                        : cv::Scalar(0, 0, 255);   // right = red
                    cv::circle(bgr_frame, cv::Point(kp.x, kp.y), 4, color, -1);
                }
            }

            PoseResult pose_result;
            pose_result.poses.push_back(pose);
            pose_result.timestamp = frame.timestamp;
            session_mgr.record_frame(pose_result);

            server.increment_frame_count();

            // Collect all ROM angles for all 11 movements
            std::unordered_map<std::string, float> all_angles;
            std::vector<std::pair<MovementType, std::string>> movements = {
                {MovementType::ABDUCTION, "abduction"},
                {MovementType::FORWARD_FLEXION, "forward_flexion"},
                {MovementType::EXTENSION, "extension"},
                {MovementType::EXTERNAL_ROTATION, "external_rotation"},
                {MovementType::INTERNAL_ROTATION, "internal_rotation"},
                {MovementType::ADDUCTION, "adduction"},
                {MovementType::HORIZONTAL_ADDDUCTION, "horizontal_adduction"},
                {MovementType::SCAPULAR_PROTRACTION, "scapular_protraction"},
                {MovementType::SCAPULAR_RETRACTION, "scapular_retraction"},
                {MovementType::SHOULDER_ELEVATION, "shoulder_elevation"},
                {MovementType::SHOULDER_DEPRESSION, "shoulder_depression"},
            };
            for (const auto& [type, name] : movements) {
                all_angles["left_" + name] = rom_analyzer.get_rom(type, Side::LEFT).max_angle;
                all_angles["right_" + name] = rom_analyzer.get_rom(type, Side::RIGHT).max_angle;
            }

            // Text overlay on video
            auto lf = all_angles["left_forward_flexion"];
            auto ra = all_angles["right_abduction"];
            cv::putText(bgr_frame, "L-Flex:" + std::to_string(static_cast<int>(lf)) + "d",
                        cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);
            cv::putText(bgr_frame, "R-Abd:" + std::to_string(static_cast<int>(ra)) + "d",
                        cv::Point(10, 55), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2);

            std::vector<uint8_t> jpeg_buf;
            cv::imencode(".jpg", bgr_frame, jpeg_buf);
            server.push_frame(jpeg_buf);

            server.update_angles(all_angles);

            auto rom_result = rom_analyzer.get_result();
            server.update_result(rom_result);

            if (frame_count % 30 == 0) {
                spdlog::info("Frame {}: L-flex={:.0f}° R-flex={:.0f}° L-abd={:.0f}° R-abd={:.0f}°",
                    frame_count,
                    all_angles["left_forward_flexion"], all_angles["right_forward_flexion"],
                    all_angles["left_abduction"], all_angles["right_abduction"]);
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
    std::string model_path;
    int port = 8080;
    bool diagnose = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--video" && i + 1 < argc) {
            video_path = argv[++i];
        } else if (arg == "--model" && i + 1 < argc) {
            model_path = argv[++i];
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
    if (!video_path.empty()) return run_demo(video_path, port, model_path);

    print_usage();
    return 0;
}