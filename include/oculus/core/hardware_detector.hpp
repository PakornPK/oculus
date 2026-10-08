#pragma once

#include <string>
#include <vector>

namespace oculus {

struct HardwareInfo {
    std::string arch;
    std::string os;
    std::string hostname;
    std::string cpu_model;
    int cpu_cores = 0;

    bool has_nvidia_gpu = false;
    bool has_intel_gpu = false;
    std::string gpu_model;
    size_t gpu_memory_mb = 0;

    bool has_npu = false;
    std::string npu_type;

    size_t total_memory_mb = 0;
    size_t available_memory_mb = 0;

    std::vector<std::string> available_backends;
    std::string selected_backend;
    bool gpu_accelerated = false;
};

struct DiagnosticResult {
    std::string component;
    bool ok = false;
    std::string message;
};

class HardwareDetector {
public:
    static HardwareInfo detect();
    static std::vector<DiagnosticResult> diagnose();

private:
    static std::string detect_arch();
    static std::string detect_os();
    static std::string detect_hostname();
    static std::string detect_cpu_model();
    static int detect_cpu_cores();
    static bool detect_nvidia_gpu(std::string& model, size_t& memory_mb);
    static bool detect_intel_gpu();
    static bool detect_npu(std::string& npu_type);
    static size_t detect_total_memory();
    static size_t detect_available_memory();
    static std::vector<std::string> detect_backends();
    static std::string select_best_backend(const std::vector<std::string>& backends);
};

} // namespace oculus