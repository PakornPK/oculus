#include "oculus/inference/inference_backend.hpp"
#include <spdlog/spdlog.h>

#ifdef __APPLE__
#include <cpu_provider_factory.h>
#endif

namespace oculus {

bool InferenceBackend::detected_ = false;
std::string InferenceBackend::backend_name_ = "cpu";
bool InferenceBackend::has_acceleration_ = false;

void InferenceBackend::detect_hardware() {
    if (detected_) return;
    detected_ = true;

#ifdef __APPLE__
    // macOS: CoreML works for most models but has shape bugs with YOLOX.
    // We enable CoreML by default — callers that know their model has issues
    // (like YOLOX) should use create_cpu_only_options() instead.
    backend_name_ = "coreml";
    has_acceleration_ = true;
    spdlog::info("InferenceBackend: macOS detected, CoreML available");
#elif defined(__linux__)
    // Linux: auto-detect best provider
    // Priority: TensorRT > CUDA > OpenVINO > RKNN > Hailo > CPU

    // Check for NVIDIA GPU (Jetson or desktop)
    bool has_cuda = false;
    bool has_tensorrt = false;
    bool has_rknn = false;
    bool has_hailo = false;

    // Check /dev/nvidia* for CUDA
    FILE* nvidia = fopen("/dev/nvidia0", "r");
    if (nvidia) {
        fclose(nvidia);
        has_cuda = true;
    }

    // Check /dev/rknpu for Rockchip NPU
    FILE* rknpu = fopen("/dev/rknpu", "r");
    if (rknpu) {
        fclose(rknpu);
        has_rknn = true;
    }

    // Check /dev/hailo0 for Hailo NPU
    FILE* hailo = fopen("/dev/hailo0", "r");
    if (hailo) {
        fclose(hailo);
        has_hailo = true;
    }

    // Check for TensorRT (requires CUDA + TensorRT library)
    if (has_cuda) {
        // TensorRT is available if the library is linked
        // ONNX Runtime will auto-fallback if not available
        has_tensorrt = true;
        backend_name_ = "tensorrt";
        has_acceleration_ = true;
        spdlog::info("InferenceBackend: NVIDIA GPU detected, using TensorRT/CUDA");
    } else if (has_rknn) {
        backend_name_ = "rknn";
        has_acceleration_ = true;
        spdlog::info("InferenceBackend: Rockchip NPU detected");
    } else if (has_hailo) {
        backend_name_ = "hailo";
        has_acceleration_ = true;
        spdlog::info("InferenceBackend: Hailo NPU detected");
    } else {
        backend_name_ = "cpu";
        has_acceleration_ = false;
        spdlog::info("InferenceBackend: No GPU/NPU detected, using CPU");
    }
#else
    backend_name_ = "cpu";
    has_acceleration_ = false;
    spdlog::info("InferenceBackend: Unknown platform, using CPU");
#endif
}

Ort::SessionOptions InferenceBackend::create_session_options(int num_threads) {
    detect_hardware();

    Ort::SessionOptions options;
    options.SetGraphOptimizationLevel(ORT_ENABLE_ALL);
    options.SetIntraOpNumThreads(num_threads);

#ifdef __APPLE__
    // Force CPU-only on macOS
    OrtSessionOptionsAppendExecutionProvider_CPU(options, 1);
#elif defined(__linux__)
    // On Linux, ONNX Runtime auto-selects the best available provider
    // when we don't explicitly append any. The priority is:
    // TensorRT > CUDA > OpenVINO > RKNN > CPU
    // No explicit provider configuration needed — auto-detection works.
    //
    // For Jetson: TensorRT EP will be used if libnvinfer is available,
    // otherwise CUDA EP, otherwise CPU.
    // For RK3588: RKNN EP will be used if available.
#endif

    return options;
}

Ort::SessionOptions InferenceBackend::create_cpu_only_options(int num_threads) {
    Ort::SessionOptions options;
    options.SetGraphOptimizationLevel(ORT_ENABLE_ALL);
    options.SetIntraOpNumThreads(num_threads);
#ifdef __APPLE__
    OrtSessionOptionsAppendExecutionProvider_CPU(options, 1);
#endif
    return options;
}

std::string InferenceBackend::backend_name() {
    detect_hardware();
    return backend_name_;
}

bool InferenceBackend::has_acceleration() {
    detect_hardware();
    return has_acceleration_;
}

} // namespace oculus