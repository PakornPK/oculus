#include "oculus/core/hardware_detector.hpp"
#include "oculus/core/config.hpp"
#include "oculus/camera/file_camera.hpp"
#include "oculus/camera/camera_factory.hpp"
#include "oculus/inference/onnx_inference_engine.hpp"
#include "oculus/inference/person_detector.hpp"
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
#include <deque>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

using namespace oculus;

static std::atomic<bool> g_running{true};

void signal_handler(int) { g_running = false; }

void print_usage() {
    std::cout << "Oculus v0.1.0 - Shoulder ROM Analysis\n"
              << "Usage: oculus [options]\n"
              << "  --video <path>    Run demo with video file\n"
              << "  --model <path>    Pose model (e.g., models/rtmpose-m.onnx)\n"
              << "  --det <path>      Person detector (e.g., models/yolox-s.onnx)\n"
              << "  --port <port>     Web server port (default: 8080)\n"
              << "  --diagnose        Run hardware diagnostics\n"
              << "  --help            Show this help\n";
}

int run_demo(const std::string& video_path, int port,
             const std::string& model_path, const std::string& det_model_path) {
    spdlog::info("=== Oculus Demo Mode ===");
    spdlog::info("Video: {}", video_path);
    spdlog::info("Pose Model: {}", model_path.empty() ? "(simulated)" : model_path);
    spdlog::info("Detector: {}", det_model_path.empty() ? "(center crop)" : det_model_path);
    spdlog::info("Port: {}", port);

    auto hw = HardwareDetector::detect();
    spdlog::info("Platform: {} {}", hw.arch, hw.cpu_model);

    std::unique_ptr<OnnxInferenceEngine> engine;
    if (!model_path.empty()) {
        engine = std::make_unique<OnnxInferenceEngine>();
        if (!engine->load_model(model_path)) {
            spdlog::error("Failed to load pose model: {}", model_path);
            return 1;
        }
        spdlog::info("Pose model loaded: {}", engine->backend_name());
    }

    PersonDetector detector;
    bool use_detection = false;
    if (!det_model_path.empty()) {
        if (detector.load_model(det_model_path)) {
            spdlog::info("Person detector loaded");
        } else {
            spdlog::warn("Person detector failed to load, using center crop");
        }
    }

    FileCamera camera(video_path);
    camera.set_loop(true);

    camera.set_loop(true);

    if (!camera.open()) {
        spdlog::error("Failed to open video: {}", video_path);
        return 1;
    }

    // Test person detection on first frame
    if (detector.is_loaded()) {
        auto test_frame = camera.capture();
        auto test_persons = detector.detect(test_frame, 0.3f);
        if (!test_persons.empty()) {
            use_detection = true;
            spdlog::info("Person detector working, {} persons found", test_persons.size());
        } else {
            spdlog::warn("Person detector: no detections in test frame, using center crop");
        }
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
                PoseResult result;
                if (use_detection) {
                    result = engine->infer_with_detection(frame, detector);
                } else {
                    result = engine->infer(frame);
                }
                if (!result.poses.empty()) {
                    pose = result.poses[0];
                    if (frame_count % 30 == 0) {
                        spdlog::info("Frame {}: person detected, {} keypoints",
                                     frame_count, pose.keypoints.size());
                        if (!pose.keypoints.empty()) {
                            spdlog::info("  kp[5](L-shoulder): ({:.1f}, {:.1f}) conf={:.2f}",
                                         pose.keypoints[5].x, pose.keypoints[5].y,
                                         pose.keypoints[5].confidence);
                        }
                    }
                } else {
                    pose.keypoints.resize(17);
                    if (frame_count % 30 == 0) {
                        spdlog::warn("Frame {}: no person detected", frame_count);
                    }
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

            // Scale keypoints from model space (192x256) to video frame
            float model_aspect = 192.0f / 256.0f;
            float frame_aspect = static_cast<float>(frame.width) / frame.height;

            int crop_x = 0, crop_y = 0, crop_w = frame.width, crop_h = frame.height;
            if (frame_aspect > model_aspect) {
                crop_h = frame.height;
                crop_w = static_cast<int>(crop_h * model_aspect);
                crop_x = (frame.width - crop_w) / 2;
            } else {
                crop_w = frame.width;
                crop_h = static_cast<int>(crop_w / model_aspect);
                crop_y = (frame.height - crop_h) / 2;
            }

            float sx = static_cast<float>(crop_w) / 192.0f;
            float sy = static_cast<float>(crop_h) / 256.0f;

            // Debug: log raw keypoints before scaling
            if (frame_count == 0) {
                spdlog::info("DEBUG: frame={}x{} crop=({},{},{},{}) scale=({:.2f},{:.2f})",
                    frame.width, frame.height, crop_x, crop_y, crop_w, crop_h, sx, sy);
                for (int i = 0; i < std::min(17, (int)pose.keypoints.size()); ++i) {
                    auto& kp = pose.keypoints[i];
                    spdlog::info("  kp[{}] raw=({:.1f},{:.1f}) conf={:.2f}", i, kp.x, kp.y, kp.confidence);
                }
            }

            for (auto& kp : pose.keypoints) {
                kp.x = kp.x * sx + crop_x;
                kp.y = kp.y * sy + crop_y;
            }

            // Debug: log scaled keypoints
            if (frame_count == 0) {
                spdlog::info("DEBUG: after scaling:");
                for (int i = 5; i <= 10; ++i) {
                    auto& kp = pose.keypoints[i];
                    spdlog::info("  kp[{}] scaled=({:.1f},{:.1f})", i, kp.x, kp.y);
                }
            }

            rom_analyzer.update(pose);

            cv::Mat cv_frame(frame.height, frame.width, CV_8UC3, frame.data.data());
            cv::Mat bgr_frame;
            cv::cvtColor(cv_frame, bgr_frame, cv::COLOR_RGB2BGR);

            // Debug: draw crop boundary (yellow dashed)
            cv::rectangle(bgr_frame, cv::Rect(crop_x, crop_y, crop_w, crop_h),
                          cv::Scalar(0, 255, 255), 1);

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

            // Draw keypoints with confidence-based coloring
            int high_conf = 0, med_conf = 0, low_conf = 0;
            for (int i = 0; i < pose.keypoints.size(); ++i) {
                auto& kp = pose.keypoints[i];
                if (kp.confidence > 0.5f) {
                    high_conf++;
                    cv::Scalar color = (i == 5 || i == 7 || i == 9 || i == 11 || i == 13 || i == 15)
                        ? cv::Scalar(0, 255, 0)   // left = green
                        : cv::Scalar(0, 0, 255);   // right = red
                    cv::circle(bgr_frame, cv::Point(kp.x, kp.y), 4, color, -1);
                } else if (kp.confidence > 0.3f) {
                    med_conf++;
                    cv::circle(bgr_frame, cv::Point(kp.x, kp.y), 3, cv::Scalar(0, 255, 255), -1);  // yellow = medium
                } else {
                    low_conf++;
                }
            }

            // Confidence overlay
            cv::putText(bgr_frame,
                "Conf: H=" + std::to_string(high_conf) + " M=" + std::to_string(med_conf) + " L=" + std::to_string(low_conf),
                cv::Point(10, frame.height - 10), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);

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

            // Send keypoints for 3D skeleton
            json keypoints_json = json::array();
            for (const auto& kp : pose.keypoints) {
                keypoints_json.push_back({{"x", kp.x}, {"y", kp.y}, {"confidence", kp.confidence}});
            }
            json kp_msg;
            kp_msg["type"] = "keypoints";
            kp_msg["keypoints"] = keypoints_json;
            server.broadcast_sse(kp_msg.dump());

            auto rom_result = rom_analyzer.get_result();
            server.update_result(rom_result);

            // Temporal smoothing: average last 5 frames
            static std::deque<std::unordered_map<std::string, float>> angle_history;
            angle_history.push_back(all_angles);
            if (angle_history.size() > 5) angle_history.pop_front();

            std::unordered_map<std::string, float> smoothed_angles;
            for (const auto& [key, val] : all_angles) {
                float sum = 0;
                int count = 0;
                for (const auto& hist : angle_history) {
                    auto it = hist.find(key);
                    if (it != hist.end()) { sum += it->second; count++; }
                }
                smoothed_angles[key] = (count > 0) ? sum / count : val;
            }

            // Debug: log angle calculation for first frame
            if (frame_count == 0) {
                auto& ls = pose.keypoints[5];  // left shoulder
                auto& le = pose.keypoints[7];  // left elbow
                auto& lh = pose.keypoints[11]; // left hip
                spdlog::info("DEBUG angles: L-shoulder=({:.1f},{:.1f}) L-elbow=({:.1f},{:.1f}) L-hip=({:.1f},{:.1f})",
                    ls.x, ls.y, le.x, le.y, lh.x, lh.y);

                // Manual angle calculation
                float v1x = le.x - ls.x, v1y = le.y - ls.y;
                float v2x = lh.x - ls.x, v2y = lh.y - ls.y;
                float dot = v1x * v2x + v1y * v2y;
                float mag1 = std::sqrt(v1x*v1x + v1y*v1y);
                float mag2 = std::sqrt(v2x*v2x + v2y*v2y);
                float cos_angle = dot / (mag1 * mag2);
                cos_angle = std::max(-1.0f, std::min(1.0f, cos_angle));
                float angle = std::acos(cos_angle) * 180.0f / M_PI;
                spdlog::info("DEBUG angle calc: dot={:.1f} mag1={:.1f} mag2={:.1f} cos={:.3f} angle={:.1f}°",
                    dot, mag1, mag2, cos_angle, angle);
            }

            // Accuracy metrics
            if (frame_count % 30 == 0) {
                float lf = smoothed_angles["left_forward_flexion"];
                float rf = smoothed_angles["right_forward_flexion"];
                float la = smoothed_angles["left_abduction"];
                float ra = smoothed_angles["right_abduction"];

                // Symmetry check
                float flex_sym = (lf > 0 && rf > 0) ? std::min(lf, rf) / std::max(lf, rf) * 100.0f : 0;
                float abd_sym = (la > 0 && ra > 0) ? std::min(la, ra) / std::max(la, ra) * 100.0f : 0;

                // Range check (normal: flexion 150-180°, abduction 150-180°)
                bool flex_ok = lf >= 100.0f && lf <= 190.0f;
                bool abd_ok = la >= 100.0f && la <= 190.0f;

                spdlog::info("Frame {}: conf[{},{}] flex={:.0f}/{:.0f}°(sym={:.0f}%{}) abd={:.0f}/{:.0f}°(sym={:.0f}%{})",
                    frame_count, high_conf, low_conf,
                    lf, rf, flex_sym, flex_ok ? " OK" : " WARN",
                    la, ra, abd_sym, abd_ok ? " OK" : " WARN");
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
    std::string det_model_path;
    int port = 8080;
    bool diagnose = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--video" && i + 1 < argc) {
            video_path = argv[++i];
        } else if (arg == "--model" && i + 1 < argc) {
            model_path = argv[++i];
        } else if (arg == "--det" && i + 1 < argc) {
            det_model_path = argv[++i];
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
    if (!video_path.empty()) return run_demo(video_path, port, model_path, det_model_path);

    print_usage();
    return 0;
}