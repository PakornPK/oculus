# Multi-Platform Optimization Guide

## Overview

Oculus is designed to run on any Linux device with automatic hardware detection and inference backend selection. This document covers optimization strategies for different platforms.

---

## Inference Backend Selection

### Auto-Detection Flow

```
Startup
  ↓
Detect Hardware
  ├── NVIDIA GPU → Try TensorRT, then CUDA
  ├── Intel GPU → Try OpenVINO
  ├── Rockchip NPU → Try RKNN
  ├── Hailo NPU → Try Hailo
  └── Fallback → CPU
  ↓
Select Best Available Backend
  ↓
Log Selected Backend
  ↓
Start Application
```

### Backend Priority

| Priority | Backend | Hardware | Performance | Power |
|----------|---------|----------|-------------|-------|
| 1 | TensorRT FP16 | NVIDIA GPU | Excellent | Medium |
| 2 | CUDA | NVIDIA GPU | Good | Medium |
| 3 | OpenVINO | Intel GPU/NPU | Good | Low-Medium |
| 4 | RKNN | Rockchip NPU | Good | Low |
| 5 | Hailo | Raspberry Pi AI Kit | Good | Low |
| 6 | CPU | Any | Basic | Low |

### ONNX Runtime Execution Providers

ONNX Runtime provides built-in support for multiple backends:

```cpp
// Auto-detect and select best backend
bool OnnxInferenceEngine::load_model(const std::string& path) {
    Ort::SessionOptions opts;
    opts.SetGraphOptimizationLevel(ORT_ENABLE_ALL);
    
    // Try backends in priority order
    if (try_backend(opts, "tensorrt")) {
        backend_ = "tensorrt";
    } else if (try_backend(opts, "cuda")) {
        backend_ = "cuda";
    } else if (try_backend(opts, "openvino")) {
        backend_ = "openvino";
    } else {
        backend_ = "cpu";  // Always works
    }
    
    session_ = Ort::Session(env_, path.c_str(), opts);
    return true;
}
```

---

## Platform-Specific Optimization

### 1. NVIDIA Jetson (Orin Nano, Xavier NX, AGX)

**Backend**: TensorRT (best) or CUDA

**TensorRT Optimization**:
```cpp
OrtTensorRTProviderOptions trt;
trt.device_id = 0;
trt.trt_max_workspace_size = 1ULL << 30;  // 1GB
trt.trt_fp16_enable = 1;                   // Auto FP16
trt.trt_engine_cache_enable = 1;           // Cache engine
trt.trt_engine_cache_path = "/tmp/trt_cache";
opts.AppendExecutionProvider_TensorRT(trt);
```

**Performance**:
- TensorRT FP16: ~10-15ms
- CUDA: ~20-25ms
- CPU: ~100ms

**Optimization Tips**:
- Enable TensorRT engine caching
- Use FP16 precision
- Enable GPU memory pooling
- Monitor temperature (throttle at 85°C)

**Power Management**:
```bash
# Set power mode
sudo nvpmodel -m 0  # Max performance
sudo nvpmodel -m 1  # Balanced
sudo nvpmodel -m 2  # Power save
```

---

### 2. Rockchip RK3588 (Orange Pi 5, Radxa Rock 5)

**Backend**: RKNN (via ONNX Runtime)

**Performance**:
- RKNN: ~20-30ms
- CPU: ~150-200ms

**Optimization Tips**:
- Use RKNN INT8 quantization
- Enable NPU frequency boost
- Monitor temperature

**NPU Configuration**:
```bash
# Check NPU status
cat /sys/kernel/debug/rknpu/version

# Set NPU frequency
echo performance > /sys/class/devfreq/fdab0000.npu/governor
```

---

### 3. Raspberry Pi 5 + AI Kit

**Backend**: Hailo (if AI Kit installed) or CPU

**Performance**:
- Hailo: ~15-25ms
- CPU: ~200-300ms

**Optimization Tips**:
- Install Hailo driver and runtime
- Use Hailo Model Zoo for optimized models
- Enable GPU memory split

**Hailo Setup**:
```bash
# Install Hailo runtime
sudo apt install hailort hailo-tappas

# Check Hailo device
hailortcli scan

# Run inference
hailortcli run-onnx rtmpose.onnx
```

---

### 4. Intel x86 (NUC, Mini PC)

**Backend**: OpenVINO (best) or CPU

**OpenVINO Optimization**:
```cpp
// OpenVINO EP
opts.AppendExecutionProvider_OpenVINO();

// Or with specific device
OrtOpenVINOProviderOptions ov;
ov.device_type = "GPU";  // or "CPU", "GPU", "MYRIAD"
opts.AppendExecutionProvider_OpenVINO(ov);
```

**Performance**:
- OpenVINO GPU: ~15-20ms
- OpenVINO CPU: ~25-35ms
- ONNX Runtime CPU: ~50-80ms

**Optimization Tips**:
- Use OpenVINO Model Optimizer
- Enable GPU inference (if Intel GPU available)
- Use INT8 quantization

**OpenVINO Setup**:
```bash
# Install OpenVINO
sudo apt install openvino-2024.0.0

# Optimize model
mo --input_model rtmpose.onnx --output_dir optimized/

# Benchmark
benchmark_app -m optimized/rtmpose.xml -d GPU
```

---

### 5. Generic ARM64 (Any ARM SBC)

**Backend**: CPU (fallback)

**Performance**:
- CPU: ~150-300ms (depends on CPU)

**Optimization Tips**:
- Use ONNX Runtime with XNNPACK delegate
- Enable multi-threading
- Optimize model size (quantization)
- Reduce input resolution

**CPU Optimization**:
```cpp
// Set number of threads
opts.SetIntraOpNumThreads(4);
opts.SetInterOpNumThreads(2);

// Enable XNNPACK
// (ONNX Runtime uses XNNPACK by default on ARM)
```

---

### 6. Generic x86 (Any x86 PC)

**Backend**: CPU (fallback)

**Performance**:
- CPU: ~50-100ms (depends on CPU)

**Optimization Tips**:
- Use ONNX Runtime with MKL-DNN
- Enable AVX2/AVX-512 if available
- Use multi-threading

---

## Performance Targets by Platform

| Platform | Backend | Target Latency | FPS | Power |
|----------|---------|---------------|-----|-------|
| Jetson Orin Nano | TensorRT FP16 | <15ms | 60+ | 15W |
| Jetson Xavier NX | TensorRT FP16 | <15ms | 60+ | 20W |
| RK3588 | RKNN INT8 | <25ms | 40+ | 10W |
| RPi 5 + Hailo | Hailo | <20ms | 50+ | 12W |
| Intel NUC | OpenVINO GPU | <20ms | 50+ | 25W |
| Intel NUC | OpenVINO CPU | <30ms | 33+ | 15W |
| Generic ARM64 | CPU | <150ms | 7+ | 5W |
| Generic x86 | CPU | <80ms | 12+ | 15W |

---

## Hardware Detection Implementation

### Detection Code

```cpp
HardwareInfo HardwareDetector::detect() {
    HardwareInfo info;
    
    // Detect architecture
    info.arch = detect_arch();  // "x86_64" or "aarch64"
    info.os = detect_os();      // "linux"
    
    // Detect CPU
    info.cpu_model = get_cpu_model();
    info.cpu_cores = std::thread::hardware_concurrency();
    
    // Detect GPU
    info.has_nvidia_gpu = check_command("nvidia-smi");
    info.has_intel_gpu = check_command("lspci | grep -i intel");
    
    // Detect NPU
    info.has_npu = check_file("/dev/rknpu") || 
                   check_file("/dev/hailo0");
    
    // Detect memory
    info.total_memory_mb = get_total_memory();
    info.available_memory_mb = get_available_memory();
    
    // Detect available backends
    info.available_backends = detect_backends();
    
    // Recommend backend
    info.recommended_backend = recommend_backend(info);
    
    return info;
}

std::vector<std::string> detect_backends() {
    std::vector<std::string> backends;
    
    // Try TensorRT
    if (try_create_session("tensorrt")) {
        backends.push_back("tensorrt");
    }
    
    // Try CUDA
    if (try_create_session("cuda")) {
        backends.push_back("cuda");
    }
    
    // Try OpenVINO
    if (try_create_session("openvino")) {
        backends.push_back("openvino");
    }
    
    // CPU always available
    backends.push_back("cpu");
    
    return backends;
}
```

### Startup Logging

```
=== Oculus v0.1.0 ===
Platform: linux aarch64
Hostname: oculus-device-001
CPU: ARM Cortex-A76 (8 cores)
Memory: 8192 MB (6144 MB available)
GPU: NVIDIA Orin (8192 MB VRAM)
NPU: none

Detecting inference backends...
  [✓] TensorRT EP - available
  [✓] CUDA EP - available
  [✗] OpenVINO EP - not available
  [✗] RKNN EP - not available
  [✓] CPU EP - available

Selected backend: TensorRT
GPU acceleration: enabled

=== Oculus Ready ===
Web UI: http://oculus.local
API: http://oculus.local:8080
```

---

## Optimization Checklist

### General (All Platforms)

- [ ] Enable ONNX Runtime graph optimization
- [ ] Use appropriate number of threads
- [ ] Enable memory pattern optimization
- [ ] Use bounded queues for frame pipeline
- [ ] Monitor memory usage over time

### NVIDIA Jetson

- [ ] Enable TensorRT engine caching
- [ ] Use FP16 precision
- [ ] Set appropriate power mode
- [ ] Monitor temperature
- [ ] Enable GPU memory pooling

### Rockchip RK3588

- [ ] Use RKNN INT8 quantization
- [ ] Enable NPU frequency boost
- [ ] Monitor temperature
- [ ] Use NPU-dedicated memory

### Raspberry Pi 5

- [ ] Install Hailo driver and runtime
- [ ] Use Hailo Model Zoo models
- [ ] Enable GPU memory split
- [ ] Monitor temperature

### Intel x86

- [ ] Use OpenVINO Model Optimizer
- [ ] Enable GPU inference (if available)
- [ ] Use INT8 quantization
- [ ] Enable AVX2/AVX-512

### CPU Fallback

- [ ] Use XNNPACK (ARM) or MKL-DNN (x86)
- [ ] Enable multi-threading
- [ ] Optimize model size
- [ ] Reduce input resolution if needed

---

## Troubleshooting

### Backend Not Detected

**Symptom**: Log shows "Using CPU backend" on NVIDIA/Intel device

**Solutions**:
1. Check if GPU drivers are installed
2. Check if ONNX Runtime EP is installed
3. Check if model is compatible with EP
4. Check logs for specific error

### Slow Inference

**Symptom**: Inference latency >100ms

**Solutions**:
1. Check which backend is selected
2. Enable FP16/INT8 precision
3. Reduce input resolution
4. Enable engine caching
5. Check for thermal throttling

### High Memory Usage

**Symptom**: Memory grows over time

**Solutions**:
1. Check for memory leaks
2. Enable bounded queues
3. Monitor with health endpoint
4. Check for unbounded caches

### Thermal Throttling

**Symptom**: Performance degrades over time

**Solutions**:
1. Add active cooling (heatsink + fan)
2. Reduce power mode
3. Enable auto-throttle
4. Improve ventilation

---

## Resources

- [ONNX Runtime Documentation](https://onnxruntime.ai/docs/)
- [TensorRT Documentation](https://developer.nvidia.com/tensorrt)
- [OpenVINO Documentation](https://docs.openvino.ai/)
- [RKNN Toolkit](https://github.com/airockchip/rknn-toolkit2)
- [Hailo Documentation](https://hailo.ai/developer-zone/)