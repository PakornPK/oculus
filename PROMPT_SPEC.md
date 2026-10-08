# Oculus — Project Specification

## 1. Project Overview

Oculus is a **web-based smart sensor for shoulder range of motion (ROM) analysis** on embedded Linux devices.

The product provides real-time pose estimation, joint angle measurement, and shoulder Active ROM analysis through an intuitive web interface accessible via local network.

**Product Vision:**
> "Connect to your Oculus device, open the browser, and start measuring shoulder range of motion in real-time. No cloud, no complexity, no vendor lock-in."

**Key Design Principle:**
> Hardware-agnostic. Oculus runs on any Linux ARM64/x86 device with automatic hardware detection and inference backend selection.

The initial development target is:

* Development machine: Apple Silicon Mac
* Target platforms: Any Linux ARM64/x86 device
* Camera: Orbbec Astra Pro (or any supported camera)
* OS target: Linux (ARM64 primary, x86 secondary)
* Initial inference model: RTMPose
* Model format: ONNX
* Inference strategy: ONNX Runtime with auto-detected execution provider
* Initial storage: SQLite
* Web server: Embedded HTTP server with WebSocket
* Language: C++20
* Build system: CMake
* Runtime packaging: single executable
* UI: Web-based (HTML/CSS/JS)

**No hardware vendor lock-in.** Oculus automatically detects available hardware and selects the best inference backend.

---

# 2. Core Principles

## 2.1 Hardware Agnostic

Oculus must not be architecturally coupled to any specific hardware vendor.

Do not make vendor-specific APIs part of the application layer.

Inference must be abstracted behind an interface so that the implementation can support:

* CPU (always available, fallback)
* CUDA (NVIDIA GPUs)
* TensorRT (NVIDIA optimized)
* OpenVINO (Intel)
* RKNN (Rockchip NPU)
* Hailo (Raspberry Pi AI Kit)
* Any ONNX-compatible backend

The application must not know which inference backend is being used.

## 2.2 Runtime Hardware Detection

Oculus must detect available hardware at runtime and select the best inference backend automatically.

```text
Startup
  ↓
Detect Hardware
  ├── Check NVIDIA GPU → Try TensorRT, then CUDA
  ├── Check Intel GPU → Try OpenVINO
  ├── Check Rockchip NPU → Try RKNN
  ├── Check Hailo NPU → Try Hailo
  └── Fallback → CPU
  ↓
Select Best Available Backend
  ↓
Log Selected Backend
  ↓
Start Application
```

**No compile-time hardware selection.** The same binary runs on any supported platform.

## 2.3 ONNX Runtime as Universal Layer

ONNX Runtime is the canonical inference interface for Oculus.

ONNX Runtime provides:
* CPU Execution Provider (always works)
* CUDA Execution Provider (NVIDIA)
* TensorRT Execution Provider (NVIDIA optimized)
* OpenVINO Execution Provider (Intel)
* CoreML Execution Provider (Apple, development)
* DirectML Execution Provider (Windows)

The application communicates with inference through:

```cpp
class InferenceEngine {
public:
    virtual ~InferenceEngine() = default;
    virtual InferenceResult infer(const Frame& frame) = 0;
    virtual bool load_model(const std::string& model_path) = 0;
    virtual std::string backend_name() const = 0;
    virtual bool is_gpu_accelerated() const = 0;
};
```

Do not expose ONNX Runtime-specific types throughout the application.

## 2.4 Fallback Strategy

Oculus must always work, even without GPU acceleration.

```text
Priority 1: TensorRT (best performance, NVIDIA)
Priority 2: CUDA (good performance, NVIDIA)
Priority 3: OpenVINO (good performance, Intel)
Priority 4: RKNN (good performance, Rockchip)
Priority 5: Hailo (good performance, Raspberry Pi)
Priority 6: CPU (always works, slower)
```

If the best backend fails, automatically fall back to the next one.

Never crash because GPU is unavailable.

---

# 3. Supported Platforms

Oculus is designed to run on any Linux device. Tested platforms include:

| Platform | Board Examples | Inference Backend | Performance |
|----------|---------------|-------------------|-------------|
| NVIDIA Jetson | Orin Nano, Xavier NX, AGX | TensorRT / CUDA | Excellent |
| Rockchip RK3588 | Orange Pi 5, Radxa Rock 5 | RKNN (via ONNX RT) | Good |
| Raspberry Pi 5 | Pi 5 + AI Kit | Hailo / CPU | Moderate |
| Intel x86 | NUC, Mini PC | OpenVINO / CPU | Good |
| Generic ARM64 | Any ARM SBC | CPU | Basic |
| Generic x86 | Any x86 PC | CPU | Basic |

**The same Oculus binary runs on all platforms.** Hardware detection happens at runtime.

---

# 4. MVP Goal

The first MVP must prove the following complete pipeline:

```text
Camera (any supported)
        |
        v
     Frame
        |
        v
   Preprocess
        |
        v
    RTMPose
        |
        v
  ONNX Runtime (auto-detected EP)
        |
        v
   Postprocess
        |
        v
 Pose Result
        |
        v
 Shoulder ROM Analysis
        |
        +--------> Web UI (Live View + Analysis)
        |
        +--------> WebSocket (real-time updates)
        |
        +--------> HTTP API
        |
        +--------> SQLite (sessions + history)
```

The MVP (v0.1) is successful when:

1. The camera can be initialized.
2. Frames can be captured continuously.
3. Hardware is auto-detected at startup.
4. Best inference backend is selected automatically.
5. RTMPose can perform inference.
6. Human keypoints can be produced.
7. Shoulder joint angles can be calculated in real-time.
8. Active ROM can be measured (flexion, extension, abduction, adduction).
9. Left vs right ROM comparison is available.
10. Web UI is accessible via browser.
11. Live view shows camera feed with pose overlay.
12. Analysis page shows shoulder ROM metrics and charts.
13. Sessions can be recorded and replayed.
14. Data can be exported (CSV, JSON).
15. Runtime metrics can be exposed.
16. The application can run continuously on any supported device.
17. The application is packaged as a single executable.
18. Watchdog auto-recovers from crashes.
19. Health monitoring is active.

> **Note:** Gait analysis (gait cycle detection, step metrics, walking speed) is planned for the next phase after shoulder ROM is complete.

Do not optimize for mass production before this pipeline works.

---

# 5. Architecture

Use a layered architecture.

```text
┌─────────────────────────────────────────────────────────┐
│                    Web Browser                          │
│  Web UI (HTML/CSS/JS) + WebSocket                       │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                Application Layer                        │
│  ├── ROM Analyzer (joint angles, shoulder ROM analysis) │
│  ├── Session Manager (recording, history, export)       │
│  ├── Hardware Detector (runtime detection)              │
│  └── Web Server (REST API + WebSocket + Static Files)   │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│            Input Sources (separate implementations)     │
│  ├── FileCamera   ← test videos (MP4), dev/QA          │
│  ├── V4L2Camera   ← USB webcam, dev/demo               │
│  └── OrbbecCamera ← stereo camera, production          │
│       └── RGB + Depth (optional)                        │
│                                                         │
│  All implement Camera interface → same Frame output     │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                  Framework Layer                        │
│  ├── Camera Interface (abstract)                        │
│  ├── Inference (ONNX Runtime + auto EP selection)       │
│  ├── Storage (SQLite)                                   │
│  ├── Configuration (YAML)                               │
│  ├── Logging (structured)                               │
│  ├── Metrics (performance)                              │
│  ├── Watchdog (auto-recovery)                           │
│  └── Health Monitor (system health)                     │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                   Platform Layer                        │
│  ├── Linux (ARM64 / x86_64)                             │
│  ├── ONNX Runtime (universal inference)                 │
│  │   ├── CPU EP (always available)                      │
│  │   ├── TensorRT EP (NVIDIA, auto-detected)            │
│  │   ├── CUDA EP (NVIDIA, auto-detected)                │
│  │   ├── OpenVINO EP (Intel, auto-detected)             │
│  │   └── Other EPs (auto-detected)                      │
│  └── Camera SDKs (Orbbec, V4L2, etc.)                   │
└─────────────────────────────────────────────────────────┘
```

**Key Principle**: Processing pipeline is camera-agnostic. All camera types output the same `Frame` type. ROM Analysis, Inference, and Web UI work identically regardless of input source.

The application layer must depend on interfaces, not vendor implementations.

---

# 6. Hardware Detection

## 6.1 Detection Flow

```text
Application Startup
       ↓
HardwareDetector::detect()
       ↓
┌──────────────────────────────────────────┐
│ Check System                             │
│ ├── Architecture (x86_64, aarch64)       │
│ ├── OS (Linux, macOS)                    │
│ ├── CPU model and cores                  │
│ ├── Total memory                         │
│ └── Available memory                     │
├──────────────────────────────────────────┤
│ Check GPU                                │
│ ├── NVIDIA GPU (nvidia-smi)              │
│ ├── Intel GPU (lspci)                    │
│ ├── ARM GPU (Mali, Adreno)               │
│ └── GPU model and VRAM                   │
├──────────────────────────────────────────┤
│ Check NPU                                │
│ ├── Rockchip NPU (/dev/rknpu)            │
│ ├── Hailo NPU (/dev/hailo0)              │
│ ├── Intel NPU (/dev/accel*)              │
│ └── Other NPUs                           │
├──────────────────────────────────────────┤
│ Check ONNX Runtime EPs                   │
│ ├── Try TensorRT EP                      │
│ ├── Try CUDA EP                          │
│ ├── Try OpenVINO EP                      │
│ ├── Try RKNN EP                          │
│ └── CPU EP (always available)            │
└──────────────────────────────────────────┘
       ↓
Select Best Available Backend
       ↓
Log Hardware Info and Selected Backend
```

## 6.2 Hardware Info Structure

```cpp
struct HardwareInfo {
    // Platform
    std::string arch;           // "x86_64", "aarch64"
    std::string os;             // "linux"
    std::string hostname;
    
    // CPU
    std::string cpu_model;
    int cpu_cores;
    
    // GPU
    bool has_nvidia_gpu;
    bool has_intel_gpu;
    bool has_arm_gpu;
    std::string gpu_model;
    size_t gpu_memory_mb;
    
    // NPU
    bool has_npu;
    std::string npu_type;       // "rknn", "hailo", "intel", "none"
    
    // Memory
    size_t total_memory_mb;
    size_t available_memory_mb;
    
    // Inference
    std::vector<std::string> available_backends;
    std::string selected_backend;
    bool gpu_accelerated;
};
```

## 6.3 Startup Output

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

# 7. Camera Abstraction

Camera access must be abstracted. The processing pipeline must NOT depend on the camera type.

## 7.1 Architecture: Input Sources vs Processing Pipeline

```text
┌─────────────────────────────────────────────────────────────┐
│                    Input Sources (separate implementations)  │
│                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────────┐ │
│  │  FileCamera  │  │  V4L2Camera  │  │  OrbbecCamera     │ │
│  │  (test video)│  │  (USB webcam)│  │  (stereo camera)  │ │
│  │  RGB only    │  │  RGB only    │  │  RGB + Depth      │ │
│  └──────┬───────┘  └──────┬───────┘  └────────┬──────────┘ │
│         │                 │                    │            │
│         └─────────────────┼────────────────────┘            │
│                           │                                 │
│                     Camera Interface                        │
│                     (same abstract type)                    │
└───────────────────────────┬─────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│              Processing Pipeline (shared, camera-agnostic)   │
│                                                             │
│  Frame → Preprocess → RTMPose → Postprocess → Pose         │
│                                                ↓            │
│                                       ROM Analysis          │
│                                                ↓            │
│                                       Web UI / Export       │
└─────────────────────────────────────────────────────────────┘
```

## 7.2 Camera Interface

```cpp
class Camera {
public:
    virtual ~Camera() = default;
    virtual bool open() = 0;
    virtual void close() = 0;
    virtual Frame capture() = 0;           // Always returns RGB frame
    virtual bool is_open() const = 0;
    virtual CameraInfo info() const = 0;

    // Optional: stereo cameras only
    virtual bool has_depth() const { return false; }
    virtual DepthMap capture_depth() { return {}; }
};
```

## 7.3 Camera Implementations

```text
Camera
  ├── FileCamera        ← Test videos (MP4, AVI, MOV)
  │                        Dev & QA only
  │                        RGB only
  │
  ├── V4L2Camera        ← USB webcam (generic)
  │                        Dev & demo
  │                        RGB only
  │
  └── OrbbecCamera      ← Stereo camera (Orbbec Astra Pro)
                           Production
                           RGB + Depth (optional)
```

## 7.4 Camera Selection by Phase

| Phase | Camera | Purpose | Data |
|-------|--------|---------|------|
| Phase 0 (Demo) | FileCamera | Prove pipeline on dev machine | RGB video |
| Phase 1-5 (Core) | FileCamera | Develop & test core features | RGB video |
| Phase 6 (ROM) | FileCamera | Test ROM calculation accuracy | RGB video |
| Phase 7 (Web UI) | FileCamera | Test web display | RGB video |
| Phase 8+ (Production) | OrbbecCamera | Real hardware integration | RGB + Depth |

## 7.5 Stereo Camera Integration (Production)

Stereo camera (Orbbec Astra Pro) provides two streams:

```text
OrbbecCamera
  ├── RGB stream    → Frame (color image)
  │                    Used by: RTMPose (always)
  │
  └── Depth stream  → DepthMap (depth data)
                       Used by: (optional enhancements)
                       ├── Auto distance calibration
                       ├── 3D pose estimation (future)
                       └── Ground plane detection
```

**Key principle**: RGB stream is the primary input. Depth is optional enhancement.

The same processing pipeline works with or without depth:

```text
With FileCamera (no depth):
  Frame(RGB) → RTMPose → 2D Pose → ROM → Web UI

With OrbbecCamera (has depth):
  Frame(RGB) → RTMPose → 2D Pose → ROM → Web UI
  DepthMap   → (optional) → auto-calibration, 3D enhancement
```

## 7.6 Implementation Isolation

```text
src/camera/
├── camera.hpp              ← Interface (abstract)
├── camera_factory.hpp/cpp  ← Auto-detect & create
├── file_camera.hpp/cpp     ← Test videos (dev/QA)
├── v4l2_camera.hpp/cpp     ← USB webcam (dev/demo)
└── orbbec_camera.hpp/cpp   ← Stereo camera (production)

src/rom/                    ← Camera-agnostic (uses Camera interface)
src/inference/              ← Camera-agnostic (uses Frame)
src/http/                   ← Camera-agnostic (uses results)
```

Future cameras can be added without changing the application layer.

---

# 8. Frame Abstraction

Create a framework-level frame representation.

```cpp
struct Frame {
    std::vector<uint8_t> data;
    int width;
    int height;
    PixelFormat format;  // RGB, BGR, GRAY, DEPTH
    int64_t timestamp;
    int64_t frame_id;
};
```

Avoid leaking OpenCV-specific types into every layer unless there is a clear reason.

OpenCV may be used internally for image processing.

---

# 9. Inference Architecture

Inference must be model-independent.

```text
InferenceEngine (interface)
      |
      +-- OnnxInferenceEngine
            |
            +-- Auto-detect best EP
            +-- Load model
            +-- Run inference
            +-- Return results
```

## 9.1 ONNX Runtime EP Selection

```cpp
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

bool try_backend(Ort::SessionOptions& opts, const std::string& name) {
    try {
        if (name == "tensorrt") {
            OrtTensorRTProviderOptions trt;
            trt.trt_fp16_enable = 1;
            trt.trt_engine_cache_enable = 1;
            opts.AppendExecutionProvider_TensorRT(trt);
        } else if (name == "cuda") {
            OrtCUDAProviderOptions cuda;
            opts.AppendExecutionProvider_CUDA(cuda);
        } else if (name == "openvino") {
            opts.AppendExecutionProvider_OpenVINO();
        }
        return true;
    } catch (...) {
        return false;
    }
}
```

## 9.2 Performance Targets by Backend

| Backend | Target Latency | Power | Notes |
|---------|---------------|-------|-------|
| TensorRT FP16 | <15ms | Medium | Best for NVIDIA |
| CUDA | <25ms | Medium | Good for NVIDIA |
| OpenVINO | <20ms | Low-Medium | Best for Intel |
| RKNN | <25ms | Low | Best for Rockchip |
| CPU | <100ms | Low | Always works |

---

# 10. Shoulder ROM Analysis

## 10.1 Movements Assessed

Based on standard clinical shoulder Active ROM assessment, 11 movements are tracked:

### Group A: Elevation Movements

| # | Movement | Thai | Keypoints | Measurement | Normal ROM |
|---|----------|------|-----------|-------------|------------|
| 1 | Abduction | Arm raises sideways | elbow, shoulder, hip | Angle from vertical (frontal plane) | 0-180° |
| 2 | Forward Flexion | Arm raises forward | elbow, shoulder, hip | Angle from vertical (sagittal plane) | 0-180° |
| 3 | Extension | Arm extends backward | elbow, shoulder, hip | Angle from vertical (posterior) | 0-60° |

### Group B: Rotation Movements

| # | Movement | Thai | Keypoints | Measurement | Normal ROM |
|---|----------|------|-----------|-------------|------------|
| 4 | External Rotation | Rotates arm outward | wrist, elbow, shoulder | forearm orientation (elbow 90°) | 0-90° |
| 5 | Internal Rotation | Rotates arm inward | wrist, elbow, shoulder | forearm orientation (elbow 90°) | 0-70° |

### Group C: Horizontal & Adduction

| # | Movement | Thai | Keypoints | Measurement | Normal ROM |
|---|----------|------|-----------|-------------|------------|
| 6 | Adduction | Arm lowers to body | elbow, shoulder, hip | Angle toward body midline | 0-30° |
| 7 | Horizontal Adduction | Arm crosses horizontally | elbow, shoulder, hip | Crosses front from 90° abd | 0-130° |

### Group D: Scapular Movements

| # | Movement | Thai | Keypoints | Measurement | Normal ROM |
|---|----------|------|-----------|-------------|------------|
| 8 | Scapular Protraction | Shoulder forward | shoulder, ear (x-offset) | Shoulder moves forward relative to ear | qualitative |
| 9 | Scapular Retraction | Shoulder blades squeeze | shoulder, ear (x-offset) | Shoulder moves backward relative to ear | qualitative |

### Group E: Shoulder Elevation/Depression

| # | Movement | Thai | Keypoints | Measurement | Normal ROM |
|---|----------|------|-----------|-------------|------------|
| 10 | Shoulder Elevation | Shoulder shrug | shoulder, ear (y-distance) | Shoulder moves up relative to neutral | qualitative |
| 11 | Shoulder Depression | Shoulder press down | shoulder, hip (y-distance) | Shoulder moves down relative to neutral | qualitative |

Both left and right sides are tracked independently for all movements.

## 10.2 Angle Calculation Methods

### Method 1: Angle from Vertical Reference (Groups A, C)

Used for: Abduction, Forward Flexion, Extension, Adduction, Horizontal Adduction

```text
Input: 3 keypoints (elbow, shoulder, hip)
       ↓
Calculate upper arm vector: v_arm = shoulder→elbow
       ↓
Calculate reference vector: v_ref = shoulder→hip (vertical down)
       ↓
Calculate angle: θ = atan2(|v_arm × v_ref|, v_arm · v_ref)
       ↓
Convert to degrees
```

### Method 2: Forearm Orientation (Group B)

Used for: External Rotation, Internal Rotation

```text
Setup: Elbow bent 90°, upper arm at side (or abducted 90°)
       ↓
Input: 3 keypoints (wrist, elbow, shoulder)
       ↓
Calculate forearm vector: v_forearm = elbow→wrist
       ↓
Calculate reference: v_ref = perpendicular to upper arm in horizontal plane
       ↓
Rotation angle = angle between v_forearm and v_ref
```

### Method 3: Distance-Based (Groups D, E)

Used for: Scapular movements, Shoulder elevation/depression

```text
Setup: Record neutral position first
       ↓
Input: shoulder and ear keypoints (scapular) or shoulder and hip (elevation)
       ↓
Measure: horizontal offset (protraction/retraction)
       Measure: vertical distance change (elevation/depression)
       ↓
Compare to neutral baseline
```

## 10.3 Active ROM Analysis

The primary feature is measuring **Active Range of Motion (Active ROM)** — the range the patient can move their shoulder through voluntary effort.

Track shoulder angles over time during active movement:

* Current angle (real-time)
* Min/Max/Average angles per movement
* ROM (Range of Motion) = max_angle - min_angle
* Left vs right comparison (symmetry)
* Historical comparison across sessions

| Metric | Unit | Description |
|--------|------|-------------|
| Current Angle | degrees | Real-time shoulder angle |
| Max Angle | degrees | Maximum angle reached during movement |
| Min Angle | degrees | Minimum angle reached (resting) |
| ROM | degrees | Range of Motion (max - min) |
| ROM Symmetry % | % | Left-right symmetry (100% = perfect) |
| Active ROM | degrees | Max voluntary ROM achieved |
| Rep Count | count | Number of repetitions performed |
| Assessment | enum | normal / below_normal / limited |

## 10.4 Movement Workflow

Each movement follows the same clinical workflow:

```text
1. Resting Position (baseline)
   └── Patient stands with arm at side → measure reference angle

2. Active Movement
   └── Patient moves arm through range → track angle continuously

3. Peak Position
   └── Maximum voluntary range reached → record max angle

4. Return to Resting
   └── Patient returns arm to side → record min angle

5. Repeat 3-5 reps
   └── System calculates average ROM from best reps
```

## 10.5 Assessment Report

Per-session output:

```text
┌──────────────────────────────────────────────────┐
│           Shoulder ROM Assessment Report          │
│           Patient: [ID]  Date: [date]             │
├──────────────────────────────────────────────────┤
│                                                    │
│  LEFT SHOULDER          RIGHT SHOULDER            │
│  ─────────────          ──────────────            │
│  Abduction:    165°     Abduction:    170°        │
│  Flexion:      158°     Flexion:      162°        │
│  Extension:     52°     Extension:     48°        │
│  Ext. Rotation: 78°     Ext. Rotation: 82°        │
│  Int. Rotation: 55°     Int. Rotation: 60°        │
│  Adduction:     35°     Adduction:     38°        │
│  Horz. Add.:   125°     Horz. Add.:   130°        │
│                                                    │
│  Symmetry: 94.2%                                  │
│  Assessment: Within normal limits                 │
│                                                    │
│  Notes: Slight limitation in left internal        │
│  rotation, likely post-surgical. Recommend        │
│  follow-up in 2 weeks.                            │
└──────────────────────────────────────────────────┘
```

## 10.3 Analysis Modes

Oculus supports multiple analysis modes:

```text
Mode 1: Real-time Analysis
├── Camera → Live processing → Instant display
├── Suitable for: Live feedback, training
└── Latency: <30ms

Mode 2: Record & Analyze
├── Camera → Record session → Analyze after
├── Suitable for: Detailed analysis, comparison
└── Supports replay + re-analyze

Mode 3: Replay
├── Load recorded session → Play/Pause/Seek
├── Frame-by-frame analysis
└── Suitable for: Review, teaching

Mode 4: Import Video
├── Import video file → Analyze
├── Suitable for: Existing footage, offline analysis
└── Supports: MP4, AVI, MOV
```

```cpp
class RomAnalysisSession {
public:
    enum class Mode {
        REALTIME,       // Live camera feed
        RECORD,         // Record then analyze
        REPLAY,         // Replay recorded session
        IMPORT          // Import video file
    };
    
    void start_realtime(Camera& camera);
    void start_recording(Camera& camera);
    void stop_recording();
    void load_session(const std::string& session_id);
    void import_video(const std::string& file_path);
    
    void play();
    void pause();
    void seek(std::chrono::milliseconds timestamp);
    void step_forward();
    void step_backward();
};
```

## 10.4 Marker Modes

Oculus supports markerless and marker-assisted analysis:

```text
Mode 1: Markerless (Default)
├── Uses RTMPose (AI-based pose estimation)
├── No markers needed
├── Convenient, fast
├── Accuracy: ±2-5° (depends on camera angle/distance)
└── Suitable for: General use, clinical, fitness

Mode 2: Marker-Assisted (Optional)
├── Uses RTMPose + ArUco markers
├── Place markers on shoulder/elbow
├── Higher accuracy
├── Accuracy: ±1-2°
└── Suitable for: Research, high-precision needs

Mode 3: Hybrid (Auto-detect)
├── If markers present → use markers
├── If no markers → use RTMPose only
└── Best of both worlds
```

```cpp
enum class MarkerMode {
    MARKERLESS,         // No markers, AI only
    ARUCO,              // ArUco markers on joints
    HYBRID              // Auto-detect markers + AI fallback
};

struct MarkerConfig {
    MarkerMode mode = MarkerMode::MARKERLESS;
    int aruco_dictionary = cv::aruco::DICT_4X4_50;
    float marker_size_cm = 3.0f;
    std::vector<std::string> marked_joints = {
        "left_shoulder", "right_shoulder",
        "left_elbow", "right_elbow"
    };
};
```

## 10.5 Calibration

Multiple calibration levels for different accuracy needs:

```text
Level 1: Auto Calibration (no user input)
├── Estimates scale from body proportions
├── Uses shoulder width as reference
└── Accuracy: ±10-15% (rough estimate)

Level 2: Quick Calibration (1 minute)
├── Stand still in front of camera for 3 seconds
├── System detects body proportions automatically
└── Accuracy: ±5-10%

Level 3: Standard Calibration (3 minutes)
├── Measure patient height
├── Stand still in front of camera (front + side)
└── Accuracy: ±3-5%

Level 4: Full Calibration (5 minutes)
├── Use calibration object (ArUco board)
├── Measure camera-to-subject distance
├── Camera angle compensation
└── Accuracy: ±1-2%
```

**Calibration is simpler than gait analysis** — no walking path or ground plane needed. Only body proportions and camera angle matter for angle measurement.

## 10.6 Camera Positioning

For shoulder ROM analysis, camera positioning is important:

```text
Recommended Setup:
├── Camera at shoulder height (or slightly above)
├── 2-3 meters from subject
├── Front view for abduction/adduction
├── Side view for flexion/extension
└── Subject faces camera or stands sideways

Camera Angle Compensation:
├── Auto-detect camera angle
├── Use body proportions
├── Correct perspective distortion
└── Convert to true angle
```

## 10.7 Equipment Tiers

Oculus works with minimal equipment, but additional equipment improves accuracy:

```text
Tier 0: Minimal (no extra equipment)
├── Camera + Oculus device
├── Markerless pose estimation
├── Auto calibration
└── Accuracy: ±5-10°

Tier 1: Basic Setup (~$50-100)
├── Tripod for camera
├── Height measurement tape
└── Accuracy: ±3-5°

Tier 2: Standard Setup (~$100-200)
├── Good camera (Orbbec Astra Pro)
├── Calibration ArUco board
├── Height reference pole
└── Accuracy: ±1-3°

Tier 3: Professional Setup (~$1000+)
├── 2 cameras (front + side view)
├── ArUco markers for joints
├── Calibration frame
├── Controlled lighting
└── Accuracy: ±0.5-1°
```

| Item | Required? | Price | Purpose |
|------|-----------|-------|---------|
| Camera | ✅ Required | $100-300 | Capture video |
| Oculus Device | ✅ Required | $100-500 | Processing |
| Tripod | Recommended | $20-50 | Stable camera |
| ArUco Board | Optional | $10-30 | Precise calibration |
| Height Rod | Optional | $20-50 | Height reference |
| 2nd Camera | Optional | $100-300 | Multi-angle analysis |

---

# 11. Web Interface

## 11.1 Access

Web UI accessible via browser:

```
http://<device-ip>
http://oculus.local (mDNS)
```

## 11.2 Pages

| Page | URL | Description |
|------|-----|-------------|
| Dashboard | `/` | Overview, quick stats, hardware info |
| Live View | `/live` | Camera feed + pose overlay + angles |
| Analysis | `/analysis` | Shoulder ROM analysis, charts |
| History | `/history` | Past sessions, replay |
| Settings | `/settings` | Configuration |
| System | `/system` | Logs, metrics, diagnostics, hardware |

## 11.3 API Endpoints

```yaml
# Health & Status
GET  /api/v1/health              # Health check
GET  /api/v1/system/info         # System information (includes hardware)
GET  /api/v1/system/hardware     # Hardware detection results
GET  /api/v1/status              # Runtime status

# Camera
GET  /api/v1/camera/info         # Camera information
POST /api/v1/camera/start        # Start camera
POST /api/v1/camera/stop         # Stop camera
GET  /api/v1/camera/stream       # MJPEG stream

# Pose & ROM
GET  /api/v1/pose/current        # Current pose
GET  /api/v1/rom/angles          # Joint angles
GET  /api/v1/rom/metrics         # Shoulder ROM metrics
GET  /api/v1/rom/analysis        # ROM analysis results

# Sessions
GET    /api/v1/sessions          # List sessions
POST   /api/v1/sessions          # Create session
GET    /api/v1/sessions/:id      # Get session
DELETE /api/v1/sessions/:id      # Delete session
POST   /api/v1/sessions/:id/export  # Export session

# Configuration
GET  /api/v1/config              # Get configuration
PUT  /api/v1/config              # Update configuration

# Metrics
GET  /api/v1/metrics             # Performance metrics
```

## 11.4 WebSocket Endpoints

```
/ws/pose     → Real-time pose data
/ws/angles   → Real-time joint angles
/ws/metrics  → Real-time performance metrics
/ws/rom      → Real-time ROM updates
```

---

# 12. Performance

Performance must be measured, not assumed.

The MVP must expose at least:

* capture latency
* preprocessing latency
* inference latency
* postprocessing latency
* total pipeline latency
* FPS
* CPU usage
* memory usage
* GPU usage (if available)
* temperature (if available)
* inference backend name
* hardware acceleration status

Example:

```text
Platform: linux aarch64
Backend: TensorRT FP16
GPU accelerated: yes

capture       4 ms
preprocess    3 ms
inference    12 ms
postprocess   2 ms
-------------------
total        21 ms

FPS: 47
CPU: 35%
GPU: 45%
Memory: 1.2 GB
Temp: 62°C
```

---

# 13. Concurrency

The MVP should use a bounded pipeline.

```text
Camera Thread
      |
      v
Bounded Frame Queue
      |
      v
Inference Thread
      |
      v
ROM Analysis Thread
      |
      v
Result
      |
      +----> WebSocket (real-time)
      |
      +----> HTTP API
      |
      +----> Storage (SQLite)
```

Do not create unbounded queues.

Prefer modern C++ facilities such as:

* `std::jthread`
* `std::stop_token`
* RAII
* `std::chrono`

---

# 14. SQLite

SQLite is the local persistent storage engine.

Use it for:

* sessions (recording data)
* poses (keypoint history)
* joint angles (angle history)
* metrics/statistics
* configuration state
* device state
* hardware info
* events
* migrations

## 14.1 Durability

For field deployment durability:

```sql
PRAGMA journal_mode=WAL;
PRAGMA synchronous=FULL;
PRAGMA temp_store=MEMORY;
```

Enable auto-backup and corruption recovery.

---

# 15. Configuration

Configuration must have sane defaults.

```yaml
# configs/default.yaml

camera:
  device: "auto"  # auto, orbbec, v4l2, usb
  resolution: "640x480"
  fps: 30

inference:
  model_path: "models/rtmpose.onnx"
  backend: "auto"  # auto, tensorrt, cuda, openvino, rknn, cpu
  precision: "fp16"  # fp32, fp16, int8 (if supported)
  num_threads: 4

rom:
  # Analysis mode
  mode: "realtime"  # realtime, record, replay, import
  analysis_type: "active_rom"  # active_rom (MVP1)
  
  # Marker settings
  marker_mode: "markerless"  # markerless, aruco, hybrid
  aruco_dictionary: "DICT_4X4_50"
  marker_size_cm: 3.0
  
  # Calibration
  calibration_level: "auto"  # auto, quick, standard, full
  subject_height_m: 0.0      # 0 = auto-estimate
  
  # Analysis settings
  joints_to_track:
    - left_shoulder
    - right_shoulder
  movements:
    # Group A: Elevation
    - abduction            # Arm raises sideways
    - forward_flexion      # Arm raises forward
    - extension            # Arm extends backward
    # Group B: Rotation
    - external_rotation    # Rotates arm outward
    - internal_rotation    # Rotates arm inward
    # Group C: Horizontal
    - adduction            # Arm lowers to body
    - horizontal_adduction # Arm crosses horizontally
    # Group D: Scapular
    - scapular_protraction # Shoulder forward
    - scapular_retraction  # Shoulder blades squeeze
    # Group E: Elevation/Depression
    - shoulder_elevation   # Shoulder shrug
    - shoulder_depression  # Shoulder press down
  reps_per_movement: 3    # Number of reps per movement
  angle_smoothing: "moving_average"
  smoothing_window: 5
  min_confidence: 0.5
  
  # Camera positioning
  auto_camera_angle_compensation: true

display:
  show_skeleton: true
  show_joint_angles: true

http:
  port: 80
  api_port: 8080

network:
  hostname: "oculus"
  mdns_enabled: true

storage:
  path: "/data/oculus.db"
  auto_backup: true

logging:
  level: "info"
```

All configuration accessible via web UI.

---

# 16. Logging

Use structured, leveled logging.

At minimum:

```text
TRACE
DEBUG
INFO
WARN
ERROR
CRITICAL
```

Logs must contain enough information to diagnose:

* hardware detection
* backend selection
* camera initialization
* inference initialization
* model loading
* runtime backend
* configuration
* failures
* performance problems
* thermal events

Do not log sensitive information.

---

# 17. Build System

Use CMake.

```text
CMakePresets.json
```

The project must support:

```text
dev              # Development (macOS/Linux, any arch)
linux-arm64      # ARM64 Linux (Jetson, RK3588, RPi, etc.)
linux-x86_64     # x86_64 Linux (Intel/AMD PCs)
```

**No hardware-specific presets.** The same binary runs on all platforms.

The production device should not require:

* compiler
* CMake
* package manager
* source code

to run Oculus.

---

# 18. Development Environment

The primary development machine is Apple Silicon.

Do not build the final macOS executable.

Build Linux ARM64 artifacts using a reproducible Linux ARM64 environment.

```text
Mac (any)
   |
   v
Linux ARM64 container
   |
   v
CMake
   |
   v
ELF ARM64 binary
   |
   v
Any Linux ARM64 device
```

---

# 19. Repository Structure

```text
oculus/
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── LICENSE
│
├── cmake/
│
├── include/
│   └── oculus/
│       ├── core/
│       │   ├── frame.hpp
│       │   ├── pose.hpp
│       │   ├── config.hpp
│       │   ├── logger.hpp
│       │   └── hardware_detector.hpp
│       ├── camera/
│       │   └── camera.hpp
│       ├── inference/
│       │   └── inference_engine.hpp
│       ├── rom/
│       │   ├── joint_angle_calculator.hpp
│       │   ├── rom_analyzer.hpp
│       │   └── shoulder_rom_analyzer.hpp
│       ├── storage/
│       │   └── storage.hpp
│       ├── http/
│       │   └── web_server.hpp
│       └── runtime/
│           ├── metrics.hpp
│           ├── pipeline.hpp
│           ├── watchdog.hpp
│           └── health_monitor.hpp
│
├── src/
│   ├── core/
│   │   ├── hardware_detector.cpp
│   │   └── ...
│   ├── camera/
│   │   ├── orbbec_camera.cpp
│   │   ├── v4l2_camera.cpp
│   │   └── ...
│   ├── inference/
│   │   ├── onnx_inference_engine.cpp
│   │   └── models/rtmpose/
│   ├── rom/
│   ├── storage/
│   ├── http/
│   └── runtime/
│
├── web/
│   ├── index.html
│   ├── live.html
│   ├── analysis.html
│   ├── history.html
│   ├── settings.html
│   ├── system.html
│   ├── css/
│   ├── js/
│   └── assets/
│
├── apps/oculus-sensor/
│   └── main.cpp
│
├── tests/
├── models/
├── configs/
├── scripts/
├── docker/
└── third_party/
```

---

# 20. Dependency Philosophy

Keep dependencies minimal.

Preferred:

* C++20
* CMake
* OpenCV
* ONNX Runtime (universal inference)
* SQLite
* cpp-httplib
* nlohmann/json
* spdlog
* yaml-cpp
* Google Test

Do not introduce vendor-specific SDKs as hard dependencies.

Camera SDKs (Orbbec, etc.) should be optional and loaded dynamically if available.

---

# 21. Security

* validate HTTP input
* avoid shell execution
* avoid unsafe deserialization
* use parameterized SQL
* avoid unnecessary network exposure
* bind HTTP interfaces intentionally
* avoid running as root where practical

---

# 22. Testing

TDD (Test-Driven Development) is mandatory.

All code must be written test-first:

1. Write failing test
2. Implement minimum code to pass
3. Refactor while keeping tests green

### Unit tests

Test:

* hardware detection
* configuration
* domain objects
* joint angle calculation
* shoulder ROM analysis
* inference preprocessing
* inference postprocessing
* storage
* HTTP handlers
* error handling

### Integration tests

Test:

```text
Camera
  ↓
Inference (with mock backend)
  ↓
ROM Analysis
  ↓
Result
  ↓
Web UI
```

### Hardware tests

Test on real hardware:

* Camera integration
* Inference performance
* Backend comparison
* Long-running stability

---

# 23. Error Handling

Errors must be explicit and diagnosable.

Do not silently ignore:

* camera failures
* model loading failures
* inference failures
* database failures
* configuration errors
* network failures
* hardware detection failures

The application should fail fast during startup when a required dependency cannot initialize.

Runtime recoverable failures should use controlled recovery.

---

# 24. Field Deployment

Oculus is designed for field deployment (unattended operation).

## 24.1 Watchdog

* System watchdog (systemd integration)
* Application watchdog
* Auto-restart on crash

## 24.2 Health Monitoring

* CPU/Memory/Disk usage
* Temperature
* Camera connection
* Inference status
* Network connectivity

## 24.3 Network Resilience

* Auto-reconnect
* Offline mode
* mDNS (oculus.local)

## 24.4 Data Durability

* SQLite WAL mode
* Auto-backup
* Corruption recovery

---

# 25. Versioning

Oculus follows semantic versioning:

```text
MAJOR.MINOR.PATCH
```

Example:

```text
Oculus 1.4.0
```

---

# 26. Single Binary

The runtime must be delivered as a single executable:

```text
./oculus
```

Containing:

* executable
* web UI assets
* default configuration
* migrations

Third-party libraries should be statically linked where practical.

---

# 27. Definition of Done

The MVP is considered complete when:

```text
$ ./oculus
```

can:

1. Auto-detect hardware and select best inference backend.
2. Initialize the camera.
3. Capture frames.
4. Run RTMPose inference (on any available backend).
5. Produce pose keypoints.
6. Calculate shoulder joint angles in real-time.
7. Measure Active ROM (flexion, extension, abduction, adduction).
8. Compare left vs right shoulder ROM.
9. Serve web UI at `http://<device-ip>`.
10. Show live view with pose overlay.
11. Display real-time shoulder ROM metrics and charts.
12. Record and replay sessions.
13. Export data (CSV, JSON).
14. Report inference latency and backend info.
15. Report FPS.
16. Store required state in SQLite.
17. Expose health/status through HTTP.
18. Auto-recover from crashes (watchdog).
19. Monitor system health.
20. Run continuously without uncontrolled memory growth.
21. Run as a single Linux executable (ARM64 or x86_64).

**Every component has tests.**

**Every change has a test.**

**Every bug fix has a regression test.**

---

# 28. Final Principle

Oculus should feel like a small embedded product, not a research project.

The desired result is:

```text
        ┌─────────────────────────────────┐
        │           Oculus                │
        │                                 │
        │  Web UI (Browser)               │
        │  ├── Live View                  │
        │  ├── ROM Analysis               │
        │  ├── History                    │
        │  └── Settings                   │
        │                                 │
        │  Backend                        │
        │  ├── Camera (any)               │
        │  ├── Inference (auto-detected)  │
        │  ├── ROM Analysis               │
        │  ├── Storage (SQLite)           │
        │  ├── HTTP API + WebSocket       │
        │  ├── Metrics                    │
        │  ├── Watchdog                   │
        │  └── Health Monitor             │
        │                                 │
        └─────────────────────────────────┘
                      │
                      ▼
                 ./oculus
                      │
                      ▼
              http://oculus.local
```

One application.

One product version.

One runtime binary.

One web interface.

**Hardware-agnostic.**

**Auto-detecting.**

**Always works.**

Keep the core small.
Keep the boundaries clean.
Measure before optimizing.
Build for today's MVP without closing tomorrow's options.
Deploy on any device with confidence.