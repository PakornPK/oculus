#include "oculus/inference/mediapipe_engine.hpp"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <array>
#include <stdexcept>

#ifdef __APPLE__
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#endif

using json = nlohmann::json;

namespace oculus {

struct MediaPipeEngine::Impl {
    bool model_loaded = false;
    bool worker_running = false;
    std::string script_path;

    // Subprocess handles
    FILE* proc_stdin = nullptr;
    FILE* proc_stdout = nullptr;
    pid_t child_pid = -1;

    // Keypoint mapping: MediaPipe 33 → COCO 17
    // (handled in Python worker, we just parse the output)
};

MediaPipeEngine::MediaPipeEngine() : impl_(std::make_unique<Impl>()) {}

MediaPipeEngine::~MediaPipeEngine() {
    stop_worker();
}

bool MediaPipeEngine::load_model(const std::string& model_path) {
    // model_path is used as the path to the worker script
    impl_->script_path = model_path;
    impl_->model_loaded = true;
    spdlog::info("MediaPipe engine configured with script: {}", model_path);
    return true;
}

bool MediaPipeEngine::start_worker() {
    if (impl_->worker_running) return true;

    // Build command
    std::string cmd = "python3 -u " + impl_->script_path;

    // Open subprocess with bidirectional pipes
    impl_->proc_stdin = popen((cmd + " 2>/dev/null").c_str(), "w");
    if (!impl_->proc_stdin) {
        spdlog::error("Failed to start MediaPipe worker");
        return false;
    }

    // For reading stdout, we need a different approach
    // Use a pipe pair
    int stdout_pipe[2];
    if (pipe(stdout_pipe) != 0) {
        spdlog::error("Failed to create pipe");
        pclose(impl_->proc_stdin);
        impl_->proc_stdin = nullptr;
        return false;
    }

    // Actually, let's use a simpler approach: popen for reading
    // and a separate mechanism for writing
    // Close the simple popen approach
    if (impl_->proc_stdin) {
        pclose(impl_->proc_stdin);
        impl_->proc_stdin = nullptr;
    }

    // Use fork+exec for proper bidirectional communication
    int stdin_pipe[2], stdout_pipe2[2];
    if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe2) != 0) {
        spdlog::error("Failed to create pipes");
        return false;
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        close(stdin_pipe[1]);   // close write end of stdin pipe
        close(stdout_pipe2[0]); // close read end of stdout pipe

        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe2[1], STDOUT_FILENO);

        close(stdin_pipe[0]);
        close(stdout_pipe2[1]);

        // Redirect stderr to /dev/null
        freopen("/dev/null", "w", stderr);

        execlp("python3", "python3", "-u", impl_->script_path.c_str(), nullptr);
        _exit(1); // exec failed
    } else if (pid > 0) {
        // Parent process
        close(stdin_pipe[0]);   // close read end of stdin pipe
        close(stdout_pipe2[1]); // close write end of stdout pipe

        impl_->child_pid = pid;
        impl_->proc_stdin = fdopen(stdin_pipe[1], "w");
        impl_->proc_stdout = fdopen(stdout_pipe2[0], "r");

        if (!impl_->proc_stdin || !impl_->proc_stdout) {
            spdlog::error("Failed to open pipe files");
            stop_worker();
            return false;
        }

        // Read "ready" line
        char ready_buf[64];
        if (fgets(ready_buf, sizeof(ready_buf), impl_->proc_stdout)) {
            std::string ready(ready_buf);
            // Trim newline
            if (!ready.empty() && ready.back() == '\n') ready.pop_back();
            if (ready == "ready") {
                impl_->worker_running = true;
                spdlog::info("MediaPipe worker ready (pid={})", pid);
                return true;
            }
        }

        spdlog::error("MediaPipe worker did not send 'ready'");
        stop_worker();
        return false;
    } else {
        spdlog::error("fork() failed");
        return false;
    }
}

void MediaPipeEngine::stop_worker() {
    if (impl_->proc_stdin) {
        fclose(impl_->proc_stdin);
        impl_->proc_stdin = nullptr;
    }
    if (impl_->proc_stdout) {
        fclose(impl_->proc_stdout);
        impl_->proc_stdout = nullptr;
    }
    if (impl_->child_pid > 0) {
        int status;
        waitpid(impl_->child_pid, &status, 0);
        impl_->child_pid = -1;
    }
    impl_->worker_running = false;
}

PoseResult MediaPipeEngine::infer(const Frame& frame) {
    PoseResult result;
    result.timestamp = frame.timestamp;

    if (!impl_->worker_running || !impl_->proc_stdin || !impl_->proc_stdout) {
        return result;
    }

    // Send frame header: width (4 bytes) + height (4 bytes)
    uint32_t w = static_cast<uint32_t>(frame.width);
    uint32_t h = static_cast<uint32_t>(frame.height);

    if (fwrite(&w, 4, 1, impl_->proc_stdin) != 1 ||
        fwrite(&h, 4, 1, impl_->proc_stdin) != 1) {
        spdlog::error("Failed to write frame header");
        return result;
    }

    // Send frame data (RGB)
    size_t data_size = static_cast<size_t>(frame.width) * frame.height * 3;
    if (fwrite(frame.data.data(), 1, data_size, impl_->proc_stdin) != data_size) {
        spdlog::error("Failed to write frame data");
        return result;
    }

    if (fflush(impl_->proc_stdin) != 0) {
        spdlog::error("Failed to flush stdin");
        return result;
    }

    // Read JSON response
    char buf[4096];
    if (!fgets(buf, sizeof(buf), impl_->proc_stdout)) {
        spdlog::error("Failed to read worker response");
        return result;
    }

    // Parse JSON
    try {
        auto j = json::parse(buf);
        auto& kps = j["keypoints"];

        Pose pose;
        pose.keypoints.resize(17);

        for (int i = 0; i < 17 && i < static_cast<int>(kps.size()); ++i) {
            pose.keypoints[i].x = kps[i][0].get<float>();
            pose.keypoints[i].y = kps[i][1].get<float>();
            pose.keypoints[i].confidence = kps[i][2].get<float>();
        }

        result.poses.push_back(pose);
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse worker response: {}", e.what());
    }

    return result;
}

PoseResult MediaPipeEngine::infer_crop(const Frame& frame, const PersonBox& box) {
    PoseResult result;
    result.timestamp = frame.timestamp;

    if (!impl_->worker_running || !impl_->proc_stdin || !impl_->proc_stdout) {
        return result;
    }

    // Crop the frame around the person box (with 10% expansion)
    float bw = box.x2 - box.x1;
    float bh = box.y2 - box.y1;
    float ex = bw * 0.1f;
    float ey = bh * 0.1f;
    int cx1 = std::max(0, static_cast<int>(box.x1 - ex));
    int cy1 = std::max(0, static_cast<int>(box.y1 - ey));
    int cx2 = std::min(frame.width, static_cast<int>(box.x2 + ex));
    int cy2 = std::min(frame.height, static_cast<int>(box.y2 + ey));
    int crop_w = cx2 - cx1;
    int crop_h = cy2 - cy1;

    if (crop_w < 10 || crop_h < 10) return result;

    // Send crop frame: header with high-bit flag + offset
    uint32_t w_flagged = static_cast<uint32_t>(crop_w) | 0x80000000u;
    uint32_t h = static_cast<uint32_t>(crop_h);
    uint32_t off_x = static_cast<uint32_t>(cx1);
    uint32_t off_y = static_cast<uint32_t>(cy1);

    fwrite(&w_flagged, 4, 1, impl_->proc_stdin);
    fwrite(&h, 4, 1, impl_->proc_stdin);
    fwrite(&off_x, 4, 1, impl_->proc_stdin);
    fwrite(&off_y, 4, 1, impl_->proc_stdin);

    // Send cropped RGB data (row by row since crop may not be contiguous in memory)
    for (int row = cy1; row < cy2; ++row) {
        const uint8_t* row_ptr = frame.data.data() + (row * frame.width + cx1) * 3;
        fwrite(row_ptr, 1, crop_w * 3, impl_->proc_stdin);
    }
    fflush(impl_->proc_stdin);

    // Read response
    char buf[4096];
    if (!fgets(buf, sizeof(buf), impl_->proc_stdout)) {
        return result;
    }

    try {
        auto j = json::parse(buf);
        auto& kps = j["keypoints"];
        Pose pose;
        pose.keypoints.resize(17);
        for (int i = 0; i < 17 && i < static_cast<int>(kps.size()); ++i) {
            pose.keypoints[i].x = kps[i][0].get<float>();
            pose.keypoints[i].y = kps[i][1].get<float>();
            pose.keypoints[i].confidence = kps[i][2].get<float>();
        }
        result.poses.push_back(pose);
    } catch (const std::exception& e) {
        spdlog::error("MediaPipe parse error: {}", e.what());
    }

    return result;
}

std::string MediaPipeEngine::backend_name() const {
    return "mediapipe";
}

bool MediaPipeEngine::is_model_loaded() const {
    return impl_->model_loaded;
}

} // namespace oculus