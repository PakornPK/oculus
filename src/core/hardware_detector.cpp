#include "oculus/core/hardware_detector.hpp"
#include <spdlog/spdlog.h>

#include <array>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unistd.h>

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

namespace oculus {

namespace {

std::string exec_command(const char* cmd) {
    std::array<char, 256> buffer;
    std::string result;
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    while (!result.empty() && (result.back() == '\n' || result.back() == ' ')) {
        result.pop_back();
    }
    return result;
}

bool command_exists(const char* cmd) {
    std::string check = "which ";
    check += cmd;
    check += " > /dev/null 2>&1";
    return system(check.c_str()) == 0;
}

bool file_exists(const char* path) {
    return access(path, F_OK) == 0;
}

} // namespace

HardwareInfo HardwareDetector::detect() {
    HardwareInfo info;

    info.arch = detect_arch();
    info.os = detect_os();
    info.hostname = detect_hostname();
    info.cpu_model = detect_cpu_model();
    info.cpu_cores = detect_cpu_cores();
    info.total_memory_mb = detect_total_memory();
    info.available_memory_mb = detect_available_memory();

    info.has_nvidia_gpu = detect_nvidia_gpu(info.gpu_model, info.gpu_memory_mb);
    info.has_intel_gpu = detect_intel_gpu();
    info.has_npu = detect_npu(info.npu_type);

    info.available_backends = detect_backends();
    info.selected_backend = select_best_backend(info.available_backends);
    info.gpu_accelerated = (info.selected_backend != "cpu");

    spdlog::info("Hardware detected: {} {} cores={}, mem={}MB",
                 info.arch, info.cpu_model, info.cpu_cores, info.total_memory_mb);
    spdlog::info("Selected backend: {} (gpu={})",
                 info.selected_backend, info.gpu_accelerated);

    return info;
}

std::string HardwareDetector::detect_arch() {
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "aarch64";
#else
    return "unknown";
#endif
}

std::string HardwareDetector::detect_os() {
#if defined(__linux__)
    return "linux";
#elif defined(__APPLE__)
    return "macos";
#else
    return "unknown";
#endif
}

std::string HardwareDetector::detect_hostname() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        return hostname;
    }
    return "unknown";
}

std::string HardwareDetector::detect_cpu_model() {
#ifdef __APPLE__
    char model[256];
    size_t size = sizeof(model);
    if (sysctlbyname("machdep.cpu.brand_string", model, &size, nullptr, 0) == 0) {
        return model;
    }
    return "Apple Silicon";
#elif defined(__linux__)
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.find("model name") != std::string::npos) {
            auto pos = line.find(':');
            if (pos != std::string::npos) {
                std::string name = line.substr(pos + 2);
                return name;
            }
        }
    }
    return "unknown";
#else
    return "unknown";
#endif
}

int HardwareDetector::detect_cpu_cores() {
    int cores = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    return cores > 0 ? cores : 1;
}

bool HardwareDetector::detect_nvidia_gpu(std::string& model, size_t& memory_mb) {
    if (!command_exists("nvidia-smi")) return false;

    model = exec_command(
        "nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null");
    if (model.empty()) return false;

    std::string mem_str = exec_command(
        "nvidia-smi --query-gpu=memory.total --format=csv,noheader,nounits 2>/dev/null");
    if (!mem_str.empty()) {
        try {
            memory_mb = std::stoul(mem_str);
        } catch (...) {
            memory_mb = 0;
        }
    }

    spdlog::info("NVIDIA GPU detected: {} ({}MB)", model, memory_mb);
    return true;
}

bool HardwareDetector::detect_intel_gpu() {
#ifdef __linux__
    std::string result = exec_command(
        "lspci 2>/dev/null | grep -i 'vga.*intel\\|display.*intel'");
    return !result.empty();
#else
    return false;
#endif
}

bool HardwareDetector::detect_npu(std::string& npu_type) {
    if (file_exists("/dev/rknpu")) {
        npu_type = "rknn";
        spdlog::info("Rockchip NPU detected");
        return true;
    }
    if (file_exists("/dev/hailo0")) {
        npu_type = "hailo";
        spdlog::info("Hailo NPU detected");
        return true;
    }
    npu_type = "none";
    return false;
}

size_t HardwareDetector::detect_total_memory() {
#ifdef __APPLE__
    int64_t mem = 0;
    size_t size = sizeof(mem);
    if (sysctlbyname("hw.memsize", &mem, &size, nullptr, 0) == 0) {
        return static_cast<size_t>(mem / (1024 * 1024));
    }
#elif defined(__linux__)
    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    while (std::getline(meminfo, line)) {
        if (line.find("MemTotal") != std::string::npos) {
            std::istringstream iss(line);
            std::string label;
            size_t kb;
            iss >> label >> kb;
            return kb / 1024;
        }
    }
#endif
    return 0;
}

size_t HardwareDetector::detect_available_memory() {
#ifdef __APPLE__
    int64_t mem = 0;
    size_t size = sizeof(mem);
    if (sysctlbyname("hw.memsize", &mem, &size, nullptr, 0) == 0) {
        return static_cast<size_t>(mem / (1024 * 1024));
    }
#elif defined(__linux__)
    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    while (std::getline(meminfo, line)) {
        if (line.find("MemAvailable") != std::string::npos) {
            std::istringstream iss(line);
            std::string label;
            size_t kb;
            iss >> label >> kb;
            return kb / 1024;
        }
    }
#endif
    return 0;
}

std::vector<std::string> HardwareDetector::detect_backends() {
    std::vector<std::string> backends;
    backends.push_back("cpu");
    return backends;
}

std::string HardwareDetector::select_best_backend(
    const std::vector<std::string>& backends) {
    const std::vector<std::string> priority = {
        "tensorrt", "cuda", "openvino", "rknn", "hailo", "cpu"
    };

    for (const auto& preferred : priority) {
        for (const auto& available : backends) {
            if (preferred == available) {
                return preferred;
            }
        }
    }
    return "cpu";
}

std::vector<DiagnosticResult> HardwareDetector::diagnose() {
    std::vector<DiagnosticResult> results;

    auto info = detect();

    results.push_back({
        "CPU",
        info.cpu_cores > 0,
        info.cpu_model + " (" + std::to_string(info.cpu_cores) + " cores)"
    });

    results.push_back({
        "Memory",
        info.total_memory_mb > 0,
        std::to_string(info.total_memory_mb) + "MB total, " +
        std::to_string(info.available_memory_mb) + "MB available"
    });

    if (info.has_nvidia_gpu) {
        results.push_back({
            "NVIDIA GPU",
            true,
            info.gpu_model + " (" + std::to_string(info.gpu_memory_mb) + "MB)"
        });
    }

    results.push_back({
        "Backend",
        !info.selected_backend.empty(),
        info.selected_backend + (info.gpu_accelerated ? " (GPU)" : " (CPU)")
    });

    for (const auto& r : results) {
        spdlog::info("[DIAG] {}: {} - {}", r.component,
                     r.ok ? "OK" : "FAIL", r.message);
    }

    return results;
}

} // namespace oculus