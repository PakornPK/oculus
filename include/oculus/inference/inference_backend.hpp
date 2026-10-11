#pragma once

#include <onnxruntime_cxx_api.h>
#include <string>
#include <vector>

namespace oculus {

/// Hardware inference backend abstraction.
/// Selects the best ONNX Runtime execution provider for the current hardware.
/// Codebase calls create_session_options() — adapter handles platform details.
class InferenceBackend {
public:
    /// Create session options configured for the best available hardware.
    /// On macOS: CoreML (GPU + Neural Engine)
    /// On Jetson: TensorRT > CUDA > CPU
    /// On RK3588: RKNN > CPU
    /// On RPi: CPU (or Coral/Hailo if detected)
    static Ort::SessionOptions create_session_options(int num_threads = 4);

    /// Force CPU-only (for models with CoreML bugs, e.g. YOLOX).
    static Ort::SessionOptions create_cpu_only_options(int num_threads = 4);

    /// Get the name of the selected backend (for logging).
    static std::string backend_name();

    /// Check if GPU/NPU acceleration is available.
    static bool has_acceleration();

private:
    static void detect_hardware();
    static bool detected_;
    static std::string backend_name_;
    static bool has_acceleration_;
};

} // namespace oculus