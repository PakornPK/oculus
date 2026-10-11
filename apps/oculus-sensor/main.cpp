#include "oculus/core/hardware_detector.hpp"
#include "oculus/core/config.hpp"
#include "oculus/camera/file_camera.hpp"
#include "oculus/camera/camera_factory.hpp"
#include "oculus/inference/onnx_inference_engine.hpp"
#include "oculus/inference/mediapipe_engine.hpp"
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
              << "  --camera [id]     Run with webcam (default: 0)\n"
              << "  --model <path>    Pose model (e.g., models/rtmpose-m.onnx)\n"
              << "  --det <path>      Person detector (e.g., models/yolox-s.onnx)\n"
              << "  --engine <name>   Inference engine: rtmpose (default), mediapipe\n"
              << "  --port <port>     Web server port (default: 8080)\n"
              << "  --diagnose        Run hardware diagnostics\n"
              << "  --help            Show this help\n";
}

int run_demo(const std::string& video_path, int port,
             const std::string& model_path, const std::string& det_model_path,
             const std::string& engine_name = "rtmpose") {
    spdlog::info("=== Oculus Demo Mode ===");
    spdlog::info("Video: {}", video_path);
    spdlog::info("Engine: {}", engine_name);
    spdlog::info("Pose Model: {}", model_path.empty() ? "(simulated)" : model_path);
    spdlog::info("Detector: {}", det_model_path.empty() ? "(center crop)" : det_model_path);
    spdlog::info("Port: {}", port);

    auto hw = HardwareDetector::detect();
    spdlog::info("Platform: {} {}", hw.arch, hw.cpu_model);

    // MediaPipe engine (self-contained: includes its own person detection)
    std::unique_ptr<MediaPipeEngine> mp_engine;
    bool use_mediapipe = (engine_name == "mediapipe");

    std::unique_ptr<OnnxInferenceEngine> engine;
    if (use_mediapipe) {
        mp_engine = std::make_unique<MediaPipeEngine>();
        // model_path is used as the worker script path
        // Try multiple paths: relative to binary, relative to cwd
        std::string script;
        if (!model_path.empty()) {
            script = model_path;
        } else {
            // Try paths relative to build/ directory
            std::vector<std::string> candidates = {
                "../scripts/mediapipe_worker.py",
                "scripts/mediapipe_worker.py",
                "../../scripts/mediapipe_worker.py",
            };
            for (const auto& c : candidates) {
                std::ifstream test(c);
                if (test.good()) { script = c; break; }
            }
            if (script.empty()) {
                spdlog::error("Cannot find mediapipe_worker.py");
                return 1;
            }
        }
        if (!mp_engine->load_model(script)) {
            spdlog::error("Failed to configure MediaPipe engine: {}", script);
            return 1;
        }
        if (!mp_engine->start_worker()) {
            spdlog::error("Failed to start MediaPipe worker");
            return 1;
        }
        spdlog::info("MediaPipe engine ready");
    } else if (!model_path.empty()) {
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
            use_detection = true;
        } else {
            spdlog::warn("Person detector failed to load, using center crop");
        }
    }
    // For MediaPipe: worker handles its own person detection (YOLOX in Python)
    // No need for C++ PersonDetector when using MediaPipe engine

    // Camera source: video file or webcam
    std::unique_ptr<Camera> camera_ptr;
    bool is_webcam = video_path.substr(0, 7) == "camera:";

    if (is_webcam) {
        std::string cam_id = video_path.substr(7);
        CameraConfig cam_config;
        cam_config.device = cam_id;
        cam_config.width = 640;
        cam_config.height = 480;
        cam_config.fps = 30;
        camera_ptr = CameraFactory::create(cam_config);
        if (!camera_ptr) {
            spdlog::error("Failed to open webcam: {}", cam_id);
            return 1;
        }
    } else {
        camera_ptr = CameraFactory::create_file_camera(video_path);
        static_cast<FileCamera*>(camera_ptr.get())->set_loop(true);
    }

    Camera& camera = *camera_ptr;

    if (!camera.open()) {
        spdlog::error("Failed to open camera: {}", video_path);
        return 1;
    }

    // Test person detection on first frame (skip for MediaPipe — always uses detection)
    if (!use_mediapipe && detector.is_loaded()) {
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
            if (use_mediapipe && mp_engine) {
                // MediaPipe worker handles YOLOX detection + pose internally
                PoseResult result = mp_engine->infer(frame);
                if (!result.poses.empty()) {
                    pose = result.poses[0];
                    if (frame_count % 30 == 0) {
                        spdlog::info("Frame {}: MediaPipe detected, {} keypoints",
                                     frame_count, pose.keypoints.size());
                    }
                } else {
                    pose.keypoints.resize(17);
                    if (frame_count % 30 == 0) {
                        spdlog::warn("Frame {}: MediaPipe no detection", frame_count);
                    }
                }
            } else if (engine) {
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

            // Note: when using infer_with_detection(), keypoints are already
            // in original frame coordinates (scaled inside OnnxInferenceEngine).
            // No additional scaling needed.

            if (frame_count == 0) {
                spdlog::info("DEBUG: frame={}x{}", frame.width, frame.height);
                for (int i = 5; i <= 10; ++i) {
                    auto& kp = pose.keypoints[i];
                    spdlog::info("  kp[{}] scaled=({:.1f},{:.1f}) conf={:.2f}", i, kp.x, kp.y, kp.confidence);
                }
            }

            auto t_pp0 = std::chrono::steady_clock::now();
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

            // Confidence overlay removed per user request

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
                all_angles["left_" + name] = rom_analyzer.get_current_angle(type, Side::LEFT);
                all_angles["right_" + name] = rom_analyzer.get_current_angle(type, Side::RIGHT);
            }


            auto t_enc0 = std::chrono::steady_clock::now();
            if (server.has_clients()) {
                std::vector<uint8_t> jpeg_buf;
                cv::imencode(".jpg", bgr_frame, jpeg_buf);
                server.push_frame(jpeg_buf);
            }
            server.update_angles(all_angles);
            auto t_enc1 = std::chrono::steady_clock::now();
            if (frame_count % 30 == 0) {
                spdlog::info("Encode+server: {:.1f}ms",
                    std::chrono::duration<double, std::milli>(t_enc1-t_enc0).count());
            }

            // Send keypoints + frame dimensions for accurate overlay
            json keypoints_json = json::array();
            for (const auto& kp : pose.keypoints) {
                keypoints_json.push_back({{"x", kp.x}, {"y", kp.y}, {"confidence", kp.confidence}});
            }
            json kp_msg;
            kp_msg["type"] = "keypoints";
            kp_msg["keypoints"] = keypoints_json;
            kp_msg["frame_width"] = frame.width;
            kp_msg["frame_height"] = frame.height;
            server.broadcast_sse(kp_msg.dump());

            auto rom_result = rom_analyzer.get_result();
            server.update_result(rom_result);
            auto t_pp1 = std::chrono::steady_clock::now();
            if (frame_count % 30 == 0) {
                spdlog::info("Per-frame: {:.1f}ms (ROM {:.1f} + enc {:.1f} + SSE {:.1f})",
                    std::chrono::duration<double, std::milli>(t_pp1-t_pp0).count(),
                    std::chrono::duration<double, std::milli>(t_enc0-t_pp0).count(),
                    std::chrono::duration<double, std::milli>(t_enc1-t_enc0).count(),
                    std::chrono::duration<double, std::milli>(t_pp1-t_enc1).count());
            }

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

        // No sleep — run at full speed (video mode or realtime)
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
    std::string camera_id;
    std::string model_path;
    std::string det_model_path;
    std::string engine_name = "rtmpose";
    int port = 8080;
    bool diagnose = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--video" && i + 1 < argc) {
            video_path = argv[++i];
        } else if (arg == "--camera") {
            camera_id = "0";
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                camera_id = argv[++i];
            }
        } else if (arg == "--model" && i + 1 < argc) {
            model_path = argv[++i];
        } else if (arg == "--det" && i + 1 < argc) {
            det_model_path = argv[++i];
        } else if (arg == "--engine" && i + 1 < argc) {
            engine_name = argv[++i];
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
    if (!video_path.empty()) return run_demo(video_path, port, model_path, det_model_path, engine_name);
    if (!camera_id.empty()) return run_demo("camera:" + camera_id, port, model_path, det_model_path, engine_name);

    print_usage();
    return 0;
}