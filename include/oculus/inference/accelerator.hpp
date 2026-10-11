#pragma once

#include "oculus/core/frame.hpp"
#include "oculus/core/pose.hpp"
#include <string>
#include <vector>
#include <memory>

namespace oculus {

/// Detected accelerator hardware type.
enum class AcceleratorType {
    CPU,           // Generic CPU fallback
    GPU_CUDA,      // NVIDIA CUDA (Jetson, desktop)
    GPU_TENSORRT,  // NVIDIA TensorRT (Jetson)
    GPU_COREML,    // Apple CoreML (macOS)
    GPU_METAL,     // Apple Metal (macOS)
    GPU_OPENCL,    // OpenCL (generic)
    NPU_RKNN,      // Rockchip NPU (Orange Pi 5, Banana Pi)
    NPU_HAILO,     // Hailo-8 NPU (RPi hat, PCIe)
    NPU_CORAL,     // Google Coral Edge TPU (USB/M.2)
    NPU_NNAPI,     // Android NNAPI
    NPU_ETHOS,     // Arm Ethos-U NPU
};

/// Describes available acceleration on this device.
struct AcceleratorInfo {
    AcceleratorType type = AcceleratorType::CPU;
    std::string name;              // Human-readable name
    std::string runtime;           // Runtime library name
    std::string model_format;      // Preferred model format (.onnx, .rknn, .tflite, etc.)
    int tops = 0;                  // Tera-operations per second (NPU)
    bool available = false;
    std::string error;             // Why not available
};

/// Auto-detect best accelerator for this hardware.
/// Priority: TensorRT > CUDA > RKNN > Hailo > Coral > CoreML > Metal > CPU
AcceleratorInfo detect_best_accelerator();

/// List all available accelerators on this device.
std::vector<AcceleratorInfo> detect_all_accelerators();

/// Get the recommended model path for this accelerator.
/// Returns empty string if no model conversion is needed.
std::string get_model_path_for_accelerator(
    AcceleratorType type,
    const std::string& base_model_path);

/// Check if MediaPipe GPU delegate works on this platform.
bool supports_mediapipe_gpu();

/// Get recommended MediaPipe model variant for this hardware.
/// Returns "lite", "full", or "heavy" based on available compute.
std::string get_recommended_mediapipe_model();

} // namespace oculus