# Oculus Implementation Plan

## Project Overview

Oculus is a **web-based smart sensor for shoulder range of motion (ROM) analysis** on embedded Linux devices. The product provides real-time pose estimation, joint angle measurement, and shoulder Active ROM analysis through an intuitive web interface.

**Key Design Principle:**
> Hardware-agnostic. Oculus runs on any Linux ARM64/x86 device with automatic hardware detection and inference backend selection.

**Product Vision:**
> "Connect to your Oculus device, open the browser, and start measuring shoulder range of motion in real-time. No cloud, no complexity, no vendor lock-in."

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    Web Browser                              │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  Web UI (HTML/CSS/JS)                               │   │
│  │  ├── Live View (camera + pose overlay)              │   │
│  │  ├── ROM Analysis (joint angles + charts)            │   │
│  │  ├── History (sessions + replay)                    │   │
│  │  ├── Settings (config)                              │   │
│  │  └── System (monitoring + hardware info)            │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Oculus Device                            │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  HTTP Server (Web + API + WebSocket)                │   │
│  └─────────────────────────────────────────────────────┘   │
│                              │                               │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  Application Layer                                  │   │
│  │  ├── Hardware Detector (runtime detection)          │   │
│  │  ├── ROM Analyzer                                   │   │
│  │  ├── Session Manager                                │   │
│  │  └── Report Generator                               │   │
│  └─────────────────────────────────────────────────────┘   │
│                              │                               │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  Framework Layer                                    │   │
│  │  ├── Camera (Orbbec, V4L2, USB)                     │   │
│  │  ├── Inference (ONNX Runtime + auto EP)             │   │
│  │  ├── Storage (SQLite)                               │   │
│  │  ├── Watchdog + Health Monitor                      │   │
│  │  └── Config + Logging + Metrics                     │   │
│  └─────────────────────────────────────────────────────┘   │
│                              │                               │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  Platform Layer                                     │   │
│  │  ├── Linux (ARM64 / x86_64)                         │   │
│  │  ├── ONNX Runtime (universal)                       │   │
│  │  │   ├── CPU EP (fallback)                          │   │
│  │  │   ├── TensorRT EP (NVIDIA, auto-detected)        │   │
│  │  │   ├── CUDA EP (NVIDIA, auto-detected)            │   │
│  │  │   ├── OpenVINO EP (Intel, auto-detected)         │   │
│  │  │   └── Other EPs (auto-detected)                  │   │
│  │  └── Camera SDKs (optional)                         │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

---

## Implementation Phases

### Phase 0: Local Web Demo (Before Board Deployment)

**Goal**: Validate the full shoulder ROM pipeline on the dev machine (Mac) using the test video, accessible via local web browser. Prove the pipeline works before deploying to embedded hardware.

**Why**: Test the complete flow (video → pose → ROM analysis → web display) without needing a physical board. Use the test video `test-data/shoulder-rom/*.mp4` for validation.

**Architecture**:

```
┌─────────────────────────────────────────────────────────┐
│                   Dev Machine (Mac)                     │
│                                                         │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐ │
│  │ Test Video  │ →  │ RTMPose     │ →  │ ROM         │ │
│  │ (FileCamera)│    │ (ONNX CPU)  │    │ Analyzer    │ │
│  └─────────────┘    └─────────────┘    └─────────────┘ │
│                                                  │      │
│                    ┌─────────────────────────────┘      │
│                    ▼                                    │
│  ┌─────────────────────────────────────────────────┐   │
│  │ Local Web Server (http://localhost:8080)         │   │
│  │  ├── Live video + pose overlay                  │   │
│  │  ├── Real-time ROM angle display                │   │
│  │  ├── ROM chart (angle over time)                │   │
│  │  └── Left vs right comparison                   │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

**Tasks**:

1. **File Camera Implementation** (`src/camera/file_camera.cpp`)
   - Read video file (MP4, AVI, MOV)
   - Frame extraction at configurable FPS
   - Loop mode for continuous testing

2. **Minimal Web Server** (reuse cpp-httplib)
   - Serve static HTML page
   - MJPEG stream endpoint (video + pose overlay)
   - WebSocket for real-time ROM data
   - Simple ROM chart (Chart.js)

3. **Pipeline Integration**
   - FileCamera → Inference (CPU) → ROM Analyzer → Web
   - Same pipeline as production, just different input source

4. **Demo Web Page** (`web/demo.html`)
   - Video player with pose skeleton overlay
   - Current angle display (left/right shoulder)
   - ROM chart (angle vs time)
   - Start/Stop/Reset controls

**Test Video**:
- Location: `test-data/shoulder-rom/*.mp4` (git-ignored)
- Content: Active shoulder ROM exercises (flexion, extension, abduction, adduction)

**How to Run**:
```bash
# Build for local development
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_DEMO=ON
cmake --build .

# Run demo with test video
./oculus-demo --video ../test-data/shoulder-rom/active-rom.mp4

# Open browser
open http://localhost:8080
```

**Deliverable**: Working web demo showing shoulder ROM analysis from test video, accessible at `http://localhost:8080`

**When**: After Phase 5 (Inference Engine) — proves the pipeline works before board deployment.

---

### Phase 1: Build System & Project Structure (Week 1)

**Goal**: Establish a working CMake build system with proper project structure.

**Tasks**:
1. Create CMakePresets.json with presets:
   - `dev` - for local development (any platform)
   - `linux-arm64` - for ARM64 Linux targets
   - `linux-x86_64` - for x86_64 Linux targets

2. Set up directory structure (see Repository Structure below)

3. Configure dependencies:
   - ONNX Runtime (universal, all platforms)
   - OpenCV
   - SQLite
   - cpp-httplib
   - nlohmann/json
   - spdlog
   - yaml-cpp
   - Google Test

4. Create initial application skeleton

**Dependencies**:
- CMake 3.15+
- C++20 compiler

**Deliverable**: Build system compiles and runs "Hello Oculus!"

---

### Phase 2: Core Framework Interfaces (Week 2)

**Goal**: Define the core abstractions.

**Tasks**:

1. **Hardware Detector Interface** (`include/oculus/core/hardware_detector.hpp`)
   ```cpp
   struct HardwareInfo {
       std::string arch;
       std::string os;
       std::string cpu_model;
       int cpu_cores;
       bool has_nvidia_gpu;
       bool has_intel_gpu;
       std::string gpu_model;
       size_t gpu_memory_mb;
       bool has_npu;
       std::string npu_type;
       size_t total_memory_mb;
       size_t available_memory_mb;
       std::vector<std::string> available_backends;
       std::string selected_backend;
       bool gpu_accelerated;
   };
   
   class HardwareDetector {
   public:
       static HardwareInfo detect();
       static std::vector<std::string> get_available_backends();
       static std::string recommend_backend(const HardwareInfo& info);
   };
   ```

2. **Frame Abstraction** (`include/oculus/core/frame.hpp`)
   ```cpp
   struct Frame {
       std::vector<uint8_t> data;
       int width;
       int height;
       PixelFormat format;
       int64_t timestamp;
       int64_t frame_id;
   };
   ```

3. **Camera Interface** (`include/oculus/camera/camera.hpp`)
   ```cpp
   class Camera {
   public:
       virtual ~Camera() = default;
       virtual bool open() = 0;
       virtual void close() = 0;
       virtual Frame capture() = 0;
       virtual bool is_open() const = 0;
   };
   ```

4. **InferenceEngine Interface** (`include/oculus/inference/inference_engine.hpp`)
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

5. **Pose Result Domain Objects** (`include/oculus/core/pose.hpp`)

6. **ROM Analysis Interfaces** (`include/oculus/rom/`)
   ```cpp
   struct JointAngle {
       std::string joint_name;
       float angle_degrees;
       float confidence;
       int64_t timestamp;
   };
   
   struct RangeOfMotion {
       std::string joint_name;
       float min_angle;
       float max_angle;
       float average_angle;
       float rom_degrees;
   };
   
   struct ShoulderROMMetrics {
       float current_angle;
       float max_angle;
       float min_angle;
       float rom_degrees;        // max - min
       float rom_symmetry_pct;   // left vs right, 100% = perfect
   };
   ```

7. **Storage Interface** (`include/oculus/storage/storage.hpp`)

8. **Configuration Interface** (`include/oculus/core/config.hpp`)

9. **Logging Interface** (`include/oculus/core/logger.hpp`)

10. **Metrics Interface** (`include/oculus/runtime/metrics.hpp`)

11. **Watchdog Interface** (`include/oculus/runtime/watchdog.hpp`)

12. **Health Monitor Interface** (`include/oculus/runtime/health_monitor.hpp`)

**Deliverable**: All interfaces defined, compiles successfully

---

### Phase 3: Hardware Detection (Week 3)

**Goal**: Implement runtime hardware detection and backend selection.

**Tasks**:

1. **Hardware Detector Implementation** (`src/core/hardware_detector.cpp`)
   - Detect architecture (x86_64, aarch64)
   - Detect CPU model and cores
   - Detect GPU (NVIDIA, Intel, ARM)
   - Detect NPU (Rockchip, Hailo, Intel)
   - Detect memory
   - Query available ONNX Runtime EPs

2. **Backend Selection Logic** (`src/inference/backend_selector.cpp`)
   ```cpp
   // Priority order
   const std::vector<std::string> BACKEND_PRIORITY = {
       "tensorrt",   // Best for NVIDIA
       "cuda",       // Good for NVIDIA
       "openvino",   // Good for Intel
       "rknn",       // Good for Rockchip
       "hailo",      // Good for RPi
       "cpu"         // Always works
   };
   
   std::string select_best_backend() {
       for (const auto& backend : BACKEND_PRIORITY) {
           if (is_backend_available(backend)) {
               return backend;
           }
       }
       return "cpu";  // Fallback
   }
   ```

3. **ONNX Runtime EP Testing** (`src/inference/ep_tester.cpp`)
   - Test each EP by trying to create a session
   - Cache results for fast startup
   - Handle failures gracefully

4. **Startup Logging**
   - Log all detected hardware
   - Log available backends
   - Log selected backend
   - Log GPU acceleration status

**Deliverable**: Hardware auto-detection works, logs hardware info at startup

---

### Phase 4: Camera Implementation (Week 4)

**Goal**: Implement camera abstraction. FileCamera first (dev/QA), production cameras later.

**Architecture**:

```text
Camera (interface)
├── FileCamera        ← Phase 4 (dev/QA, test videos)
├── V4L2Camera        ← Phase 4 (dev/demo, USB webcam)
└── OrbbecCamera      ← Phase 8+ (production, stereo RGB+Depth)
```

**Tasks**:

1. **Camera Interface** (`include/oculus/camera/camera.hpp`)
   ```cpp
   class Camera {
   public:
       virtual ~Camera() = default;
       virtual bool open() = 0;
       virtual void close() = 0;
       virtual Frame capture() = 0;           // Always RGB
       virtual bool is_open() const = 0;
       virtual CameraInfo info() const = 0;
       virtual bool has_depth() const { return false; }
       virtual DepthMap capture_depth() { return {}; }
   };
   ```

2. **FileCamera** (`src/camera/file_camera.cpp`) — Phase 4
   - Read video file (MP4, AVI, MOV)
   - Frame extraction at configurable FPS
   - Loop mode for continuous testing
   - Used for: all development and QA

3. **V4L2Camera** (`src/camera/v4l2_camera.cpp`) — Phase 4
   - Linux Video4Linux2 (generic USB cameras)
   - Used for: dev/demo with live camera

4. **OrbbecCamera** (`src/camera/orbbec_camera.cpp`) — Phase 8+
   - Orbbec SDK integration (stereo camera)
   - RGB + Depth streams
   - Used for: production on board
   - Same pipeline, just different input

5. **Camera Factory** (`src/camera/camera_factory.cpp`)
   ```cpp
   std::unique_ptr<Camera> CameraFactory::create(const CameraConfig& config) {
       if (config.device == "file") return std::make_unique<FileCamera>(config);
       if (config.device == "v4l2") return std::make_unique<V4L2Camera>(config);
       if (config.device == "orbbec") return std::make_unique<OrbbecCamera>(config);
       // Auto: FileCamera for dev, OrbbecCamera for production
       if (config.device == "auto") {
           if (is_orbbec_available()) return std::make_unique<OrbbecCamera>(config);
           if (is_v4l2_available()) return std::make_unique<V4L2Camera>(config);
           return std::make_unique<FileCamera>(config);  // Fallback
       }
   }
   ```

6. **Frame Pipeline** (`src/core/frame_pipeline.hpp/.cpp`)
   - Bounded frame queue
   - Producer-consumer pattern

7. **Camera Thread** (`src/runtime/camera_thread.hpp/.cpp`)

**Deliverable**: Camera captures frames. FileCamera + V4L2Camera work. OrbbecCamera stubbed.

**Key Principle**: Processing pipeline (ROM, inference, web UI) is camera-agnostic. Same code works with all camera types.

---

### Phase 5: Inference Engine (Week 5-6)

**Goal**: Implement ONNX Runtime inference with auto EP selection.

**Tasks**:

1. **ONNX Inference Engine** (`src/inference/onnx_inference_engine.cpp`)
   ```cpp
   bool OnnxInferenceEngine::load_model(const std::string& path) {
       Ort::SessionOptions opts;
       opts.SetGraphOptimizationLevel(ORT_ENABLE_ALL);
       
       // Auto-detect and try backends
       if (try_backend(opts, "tensorrt")) {
           backend_ = "tensorrt";
       } else if (try_backend(opts, "cuda")) {
           backend_ = "cuda";
       } else if (try_backend(opts, "openvino")) {
           backend_ = "openvino";
       } else {
           backend_ = "cpu";
       }
       
       session_ = Ort::Session(env_, path.c_str(), opts);
       return true;
   }
   ```

2. **TensorRT Configuration** (optional, auto-detected)
   ```cpp
   bool try_tensorrt(Ort::SessionOptions& opts) {
       try {
           OrtTensorRTProviderOptions trt;
           trt.trt_fp16_enable = 1;
           trt.trt_engine_cache_enable = 1;
           trt.trt_engine_cache_path = "/tmp/oculus_trt_cache";
           opts.AppendExecutionProvider_TensorRT(trt);
           return true;
       } catch (...) {
           return false;
       }
   }
   ```

3. **RTMPose Model** (`src/inference/models/rtmpose/`)
   - Preprocessor
   - Postprocessor

4. **Inference Thread** (`src/runtime/inference_thread.hpp/.cpp`)

**Deliverable**: Inference works on any available backend, auto-selects best

---

### Phase 6A: Core Shoulder ROM Analysis (Week 7)

**Goal**: Implement all 11 shoulder Active ROM movements per clinical assessment standard.

**Movements** (11 total):

| Group | Movements | Method | Difficulty |
|-------|-----------|--------|------------|
| A: Elevation | Abduction, Forward Flexion, Extension | Angle from vertical | ✅ Easy |
| B: Rotation | External Rotation, Internal Rotation | Forearm orientation | ⚠️ Medium |
| C: Horizontal | Adduction, Horizontal Adduction | Angle from vertical | ✅ Easy |
| D: Scapular | Protraction, Retraction | Shoulder-ear x-offset | ⚠️ Medium |
| E: Elevation | Shoulder Elevation, Depression | Shoulder y-distance | ✅ Easy |

**Tasks**:

1. **Joint Angle Calculator** (`src/rom/joint_angle_calculator.hpp/.cpp`)
   - Method 1: Angle from vertical (Groups A, C) — 3 keypoints
   - Method 2: Forearm orientation (Group B) — wrist, elbow, shoulder
   - Method 3: Distance-based (Groups D, E) — shoulder-ear or shoulder-hip offset
   - Angle smoothing (moving average, Kalman filter)

2. **Shoulder ROM Analyzer** (`src/rom/shoulder_rom_analyzer.hpp/.cpp`)
   - Track all 11 movements per side (left/right)
   - Min/Max/Average angles per movement
   - Left vs right comparison (symmetry %)
   - Rep detection (movement start → peak → return)
   - Active ROM measurement (max voluntary range)
   - Assessment (normal / below_normal / limited)
   - Historical comparison across sessions

3. **Movement Detector** (`src/rom/movement_detector.hpp/.cpp`)
   - Detect which movement is being performed
   - Track movement phase (rest → active → peak → return)
   - Count repetitions
   - Calculate per-rep ROM

4. **Angle Smoother** (`src/rom/angle_smoother.hpp/.cpp`)
   - Moving average (configurable window)
   - Kalman filter (optional, for noisy data)

5. **Neutral Position Calibrator** (`src/rom/neutral_calibrator.hpp/.cpp`)
   - Record resting position as baseline
   - Required for Groups D, E (scapular, elevation)
   - Quick: stand still 3 seconds

**Deliverable**: All 11 shoulder movements assessed, Active ROM tracked per movement

> **Note:** Gait cycle detection (step length, cadence, walking speed) is planned for Phase 6D after shoulder ROM is complete.

---

### Phase 6B: Analysis Modes (Week 8)

**Goal**: Support multiple analysis modes (real-time, record, replay, import).

**Tasks**:

1. **Session Manager** (`src/rom/session_manager.hpp/.cpp`)
   - Real-time mode (live camera feed)
   - Record mode (save frames + pose results + angles)
   - Replay mode (playback with play/pause/seek/step)
   - Import mode (load video files)

2. **Video Import** (`src/rom/video_importer.hpp/.cpp`)
   - Support MP4, AVI, MOV formats
   - Frame extraction at configurable FPS
   - Batch processing
   - Progress tracking

3. **Replay Controller** (`src/rom/replay_controller.hpp/.cpp`)
   - Play/Pause
   - Seek to timestamp
   - Frame-by-frame step forward/backward
   - Speed control (0.5x, 1x, 2x)

**Deliverable**: All analysis modes working, video import supported

---

### Phase 6C: Calibration & Alignment (Week 9)

**Goal**: Implement calibration and auto-alignment features.

**Tasks**:

1. **Auto Calibrator** (`src/rom/calibration/auto_calibrator.hpp/.cpp`)
   - Body proportion estimation (shoulder width → scale reference)
   - No user input required

2. **Quick Calibrator** (`src/rom/calibration/quick_calibrator.hpp/.cpp`)
   - Stand-still detection (3 seconds)
   - Automatic body measurement

3. **Standard Calibrator** (`src/rom/calibration/standard_calibrator.hpp/.cpp`)
   - Subject height input
   - Front + side view analysis

4. **Full Calibrator** (`src/rom/calibration/full_calibrator.hpp/.cpp`)
   - ArUco board detection
   - Camera intrinsics calculation
   - Precise angle measurement

5. **Camera Angle Compensator** (`src/rom/camera_angle_compensator.hpp/.cpp`)
   - Camera angle detection (perspective correction)
   - Subject orientation detection (front/side)
   - Angle compensation for accurate ROM measurement

6. **Marker Detector** (`src/rom/marker_detector.hpp/.cpp`)
   - ArUco marker detection
   - Marker position tracking on shoulder/elbow
   - Hybrid mode (markers + AI fallback)

**Deliverable**: All calibration levels working, auto-alignment active

---

### Phase 7: Web UI (Week 10-12)

**Goal**: Implement web-based user interface.

**Tasks**:

1. **Web Server** (`src/http/web_server.hpp/.cpp`)
   - Serve static files
   - REST API
   - WebSocket
   - MJPEG stream

2. **REST API** (`src/http/controllers/`)
   - Health & status
   - Camera control
   - Pose & ROM data
   - Session management
   - Configuration
   - Hardware info

3. **WebSocket** (`src/http/websocket/`)
   - Real-time pose updates
   - Real-time angle updates
   - Real-time metrics
   - Real-time ROM updates

4. **Web UI Pages** (`web/`)
   - Dashboard (overview, hardware info)
   - Live View (camera + pose + angles)
   - Analysis (shoulder ROM, charts)
   - History (sessions, replay)
   - Settings (configuration)
   - System (logs, metrics, diagnostics)

5. **Frontend**
   - HTML5 + CSS3 + Vanilla JS
   - Chart.js for graphs
   - Canvas for pose overlay
   - WebSocket for real-time updates
   - Responsive design

**Deliverable**: Full web UI accessible via browser

---

### Phase 8: Storage & Sessions (Week 12)

**Goal**: Implement SQLite storage with session management.

**Tasks**:

1. **SQLite Storage** (`src/storage/sqlite_storage.hpp/.cpp`)
   - WAL mode for durability
   - Schema for sessions, poses, angles, metrics, config
   - Auto-backup

2. **Session Manager** (`src/storage/session_manager.hpp/.cpp`)
   - Session lifecycle
   - Data recording
   - Export (CSV, JSON, PDF)

**Deliverable**: Sessions recorded, data exportable

---

### Phase 9: Field Deployment Features (Week 13-14)

**Goal**: Implement watchdog, health monitoring, and resilience.

**Tasks**:

1. **Watchdog** (`src/runtime/watchdog.hpp/.cpp`)
   - Systemd integration
   - Application watchdog
   - Auto-restart

2. **Health Monitor** (`src/runtime/health_monitor.hpp/.cpp`)
   - CPU/Memory/Disk
   - Temperature
   - Camera status
   - Inference status
   - Network

3. **Network Manager** (`src/runtime/network_manager.hpp/.cpp`)
   - Auto-reconnect
   - Offline mode
   - mDNS

4. **Systemd Service** (`scripts/oculus.service`)

**Deliverable**: System recovers from failures, monitors health

---

### Phase 10: Single Binary Packaging (Week 15)

**Goal**: Package as a single executable with embedded web assets.

**Tasks**:

1. **Resource Embedding**
   - Web UI (HTML/CSS/JS)
   - Default configuration
   - Reference external ONNX model

2. **Static Linking**
   - All third-party libraries
   - Only platform dependencies remain external

3. **Build Script** (`scripts/build.sh`)
   - Cross-compilation
   - Docker-based build
   - Output single `oculus` binary

4. **Deployment Script** (`scripts/deploy.sh`)

**Deliverable**: Single `./oculus` binary runs everything

---

### Phase 11: Testing (Week 16)

**Goal**: Comprehensive test suite (TDD enforced).

**Tasks**:

1. **Unit Tests** (already written, need implementation to pass)
   - Hardware detection tests
   - Preprocessor tests
   - Postprocessor tests
   - Joint angle calculator tests
   - Shoulder ROM analyzer tests
   - Storage tests
   - Controller tests

2. **Integration Tests**
   - Full pipeline test with mocks
   - Web API tests
   - WebSocket tests

3. **Hardware Tests**
   - Camera integration
   - Inference performance
   - Backend comparison
   - Long-running stability

**Deliverable**: All tests pass, coverage >80%

---

### Phase 12: Performance Optimization (Week 17)

**Goal**: Optimize for field deployment performance.

**Tasks**:

1. **Backend Optimization**
   - TensorRT engine caching
   - FP16/INT8 precision
   - Memory pooling

2. **Pipeline Optimization**
   - Zero-copy where possible
   - Thread affinity

3. **Web UI Optimization**
   - WebSocket compression
   - Image compression

4. **Benchmarking**
   - All backends
   - Latency, FPS, memory, power

**Deliverable**: Meets performance targets on all platforms

---

### Phase 13: Documentation & Deployment (Week 18)

**Goal**: Document everything for field deployment.

**Tasks**:

1. **User Documentation**
   - Quick start guide
   - Web UI guide
   - Shoulder ROM analysis guide
   - Multi-platform guide

2. **API Documentation**
   - REST API reference
   - WebSocket protocol

3. **Deployment Guide**
   - Hardware setup (all platforms)
   - Software installation
   - Configuration

**Deliverable**: Complete documentation

---

## Dependency Summary

| Category | Library | Purpose | Platform |
|----------|---------|---------|----------|
| Build | CMake 3.15+ | Build system | All |
| Language | C++20 | Modern features | All |
| Vision | OpenCV | Image processing | All |
| Inference | ONNX Runtime | Universal inference | All |
| Database | SQLite3 | Local storage | All |
| HTTP | cpp-httplib | Embedded server | All |
| JSON | nlohmann/json | JSON serialization | All |
| Logging | spdlog | Structured logging | All |
| Config | yaml-cpp | Configuration | All |
| Testing | Google Test | Unit/integration | All |
| Frontend | Chart.js | Web UI charts | Browser |

**No vendor-specific SDKs as hard dependencies.** Camera SDKs are optional.

---

## Directory Structure

```
oculus/
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── LICENSE
│
├── cmake/
│   ├── FindOnnxRuntime.cmake
│   └── StaticLink.cmake
│
├── include/oculus/
│   ├── core/
│   │   ├── frame.hpp
│   │   ├── pose.hpp
│   │   ├── config.hpp
│   │   ├── logger.hpp
│   │   └── hardware_detector.hpp
│   ├── camera/
│   │   └── camera.hpp
│   ├── inference/
│   │   └── inference_engine.hpp
│   ├── rom/
│   │   ├── joint_angle_calculator.hpp
│   │   ├── rom_analyzer.hpp
│   │   └── shoulder_rom_analyzer.hpp
│   ├── storage/
│   │   └── storage.hpp
│   ├── http/
│   │   └── web_server.hpp
│   └── runtime/
│       ├── metrics.hpp
│       ├── pipeline.hpp
│       ├── watchdog.hpp
│       └── health_monitor.hpp
│
├── src/
│   ├── core/
│   │   ├── hardware_detector.cpp
│   │   ├── frame.cpp
│   │   ├── pose.cpp
│   │   ├── config_manager.cpp
│   │   └── logger.cpp
│   ├── camera/
│   │   ├── camera_factory.cpp
│   │   ├── orbbec_camera.cpp
│   │   ├── v4l2_camera.cpp
│   │   └── file_camera.cpp
│   ├── inference/
│   │   ├── onnx_inference_engine.cpp
│   │   ├── backend_selector.cpp
│   │   ├── ep_tester.cpp
│   │   └── models/
│   │       └── rtmpose/
│   │           ├── rtmpose_preprocessor.cpp
│   │           └── rtmpose_postprocessor.cpp
│   ├── rom/
│   │   ├── joint_angle_calculator.cpp
│   │   ├── rom_analyzer.cpp
│   │   ├── shoulder_rom_analyzer.cpp
│   │   └── angle_smoother.cpp
│   ├── storage/
│   │   ├── sqlite_storage.cpp
│   │   └── session_manager.cpp
│   ├── http/
│   │   ├── web_server.cpp
│   │   ├── websocket/
│   │   │   ├── pose_ws.cpp
│   │   │   ├── angles_ws.cpp
│   │   │   └── metrics_ws.cpp
│   │   └── controllers/
│   │       ├── health_controller.cpp
│   │       ├── camera_controller.cpp
│   │       ├── pose_controller.cpp
│   │       ├── rom_controller.cpp
│   │       ├── session_controller.cpp
│   │       ├── config_controller.cpp
│   │       └── metrics_controller.cpp
│   └── runtime/
│       ├── camera_thread.cpp
│       ├── inference_thread.cpp
│       ├── result_pipeline.cpp
│       ├── metrics_collector.cpp
│       ├── watchdog.cpp
│       ├── health_monitor.cpp
│       └── network_manager.cpp
│
├── web/
│   ├── index.html
│   ├── live.html
│   ├── analysis.html
│   ├── history.html
│   ├── settings.html
│   ├── system.html
│   ├── css/
│   │   └── style.css
│   ├── js/
│   │   ├── app.js
│   │   ├── live.js
│   │   ├── analysis.js
│   │   ├── history.js
│   │   ├── settings.js
│   │   ├── system.js
│   │   ├── chart-utils.js
│   │   └── websocket.js
│   └── assets/
│
├── apps/oculus-sensor/
│   └── main.cpp
│
├── tests/
│   ├── unit/
│   │   ├── core/
│   │   │   └── hardware_detector_test.cpp
│   │   ├── inference/
│   │   ├── rom/
│   │   ├── storage/
│   │   └── http/
│   ├── integration/
│   │   └── pipeline_test.cpp
│   └── hardware/
│       ├── camera_test.cpp
│       └── benchmark_test.cpp
│
├── models/
│   └── rtmpose.onnx
│
├── configs/
│   └── default.yaml
│
├── scripts/
│   ├── build.sh
│   ├── deploy.sh
│   └── oculus.service
│
├── docker/
│   └── Dockerfile.cross-compile
│
└── third_party/
```

---

## Key Design Decisions

1. **Hardware Agnostic**: No vendor lock-in. Same binary runs on any Linux device.

2. **Runtime Detection**: Hardware detected at startup, best backend selected automatically.

3. **ONNX Runtime Universal Layer**: Use ONNX Runtime's built-in EP support. No need to implement multiple backends.

4. **Fallback Strategy**: Always works. CPU fallback if no GPU available.

5. **Web-Based UI**: All operations through browser. No client software needed.

6. **Bounded Pipeline**: Prevent memory growth. Configurable queue sizes.

7. **Single Binary**: Web UI embedded. Deploy with single command.

8. **Field Durability**: Watchdog, health monitoring, auto-recovery.

9. **TDD**: All code written test-first.

---

## Success Criteria (MVP Definition of Done)

The MVP is complete when:

1. ✅ Hardware auto-detected at startup
2. ✅ Best inference backend selected automatically
3. ✅ `./oculus` starts and serves web UI
4. ✅ Live view shows camera feed with pose overlay
5. ✅ Joint angles displayed in real-time
6. ✅ Shoulder Active ROM measured (flexion, extension, abduction, adduction)
7. ✅ Left vs right ROM comparison
8. ✅ Sessions can be recorded and replayed
9. ✅ Data exportable (CSV, JSON)
10. ✅ Settings configurable via web UI
11. ✅ System health monitored
12. ✅ Watchdog auto-recovers from crashes
13. ✅ Runs on any Linux ARM64/x86_64 device
14. ✅ Single binary deployment

---

## Timeline Estimate

| Phase | Description | Weeks |
|-------|-------------|-------|
| 0 | Local Web Demo (proves pipeline on Mac) | 0.5 |
| 1 | Build System & Project Structure | 1 |
| 2 | Core Framework Interfaces | 1 |
| 3 | Hardware Detection | 1 |
| 4 | Camera Implementation | 1 |
| 5 | Inference Engine + Auto EP | 2 |
| 6A | Core Shoulder ROM Analysis | 1 |
| 6B | Analysis Modes (Real-time/Record/Replay/Import) | 1 |
| 6C | Calibration & Alignment | 1 |
| 6D | Gait Analysis (future) | 1 |
| 7 | Web UI | 3 |
| 8 | Storage & Sessions | 1 |
| 9 | Field Deployment Features | 2 |
| 10 | Single Binary Packaging | 1 |
| 11 | Testing | 1 |
| 12 | Performance Optimization | 1 |
| 13 | Documentation & Deployment | 1 |

**Total Estimated Effort**: 19 weeks (4.75 months)

---

## Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| ONNX Runtime EP compatibility | High | Test on multiple platforms early |
| Camera SDK availability | Medium | V4L2 fallback for generic cameras |
| Performance on slow devices | Medium | CPU fallback, optimize preprocessing |
| Web UI performance | Medium | Lightweight JS, compression |
| Thermal issues | Medium | Health monitoring, auto-throttle |
| Network reliability | Medium | Offline mode, auto-reconnect |
| Long-term stability | High | Watchdog, health monitoring |

---

## Supported Platforms

| Platform | Board Examples | Backend | Status |
|----------|---------------|---------|--------|
| NVIDIA Jetson | Orin Nano, Xavier NX | TensorRT/CUDA | Primary |
| Rockchip RK3588 | Orange Pi 5, Radxa Rock 5 | RKNN | Supported |
| Raspberry Pi 5 | Pi 5 + AI Kit | Hailo/CPU | Supported |
| Intel x86 | NUC, Mini PC | OpenVINO/CPU | Supported |
| Generic ARM64 | Any ARM SBC | CPU | Basic |
| Generic x86 | Any x86 PC | CPU | Basic |

**Same binary runs on all platforms.** No recompilation needed.

---

## Phase 14: Stereo Depth Integration

**Goal**: Improve shoulder ROM accuracy using Orbbec Astra Pro depth camera and point cloud processing. Target: ±5-10° for simple movements, ±2-3° precision with smoothing.

**Hardware**: Orbbec Astra Pro Plus (structured light, 640x480 depth @ 30fps, ±3mm @ 1m). Note: Astra Pro Plus is in "limited maintenance" mode; consider Astra 2 for long-term SDK support.

**Chosen pathway**: SimpleDepthPose + temporal smoothing + confidence weighting (hardware-efficient, stable, near real-time).

**Architecture**:
```
RTMPose (TensorRT FP16) → 2D keypoints (3ms)
  ↓
Depth lookup at keypoint pixels (median filter, 3ms)
  ↓
3D via camera intrinsics + per-joint offset (+3cm shoulder)
  ↓
Temporal smoothing (EMA α=0.3, window=5)
  ↓
Confidence-weighted angle calculation
```

**Tasks**:

1. **Depth camera integration** (`src/camera/orbbec_camera.cpp`)
   - Orbbec SDK v1 (Astra SDK 2.1.3) or OpenNI2 for Astra Pro Plus
   - RGB + depth stream capture at 30fps
   - Depth-to-RGB alignment via SDK or OpenCV `stereoCalibrate`

2. **Calibration** (`src/rom/depth_calibrator.cpp`)
   - ChArUco board for intrinsic/extrinsic calibration
   - `cv::initUndistortRectifyMap` + `cv::remap` for real-time distortion correction
   - Per-joint depth offset calibration (shoulder +3cm starting point)

3. **SimpleDepthPose lifting** (`src/inference/depth_pose_lifter.cpp`)
   - Cross-shaped median filter at 2D keypoint pixel locations
   - Per-joint depth offsets (shoulder: +3cm, elbow: +2cm, wrist: +1cm)
   - Convert to 3D via camera intrinsics
   - ~3ms latency

4. **Temporal smoothing** (`src/rom/angle_smoother.cpp`)
   - EMA smoothing (α=0.3, window=5 frames)
   - Confidence weighting: reject keypoints <0.3 confidence
   - Reduces noise from ±5° to ±2-3° precision

5. **Point cloud shoulder geometry** (Phase 2, deferred)
   - RTMPose keypoint → depth ROI crop → point cloud extraction
   - Normal estimation (PCL `NormalEstimationUsingIntegralImages`)
   - RANSAC plane fitting for glenoid orientation
   - For internal/external rotation measurements
   - ~10-50ms latency

6. **IMU integration for rotation** (Phase 2, optional)
   - Small IMU sensor (BNO055/MPU6050) on wrist/forearm
   - BLE/USB integration for orientation data
   - ±1-2° accuracy for internal/external rotation
   - Calibrate once per session

**Accuracy targets**:
- Flexion/abduction: ±5-10° (Phase 1)
- Rotations: ±10-15° (Phase 2 with point cloud/IMU)
- Precision: ±2-3° with temporal smoothing

**Research**: See `research/orbbec-astra-pro-stereo-rom/REPORT.md` for full analysis (87 sources).

---

## Next Steps

1. Review and approve this implementation plan
2. Set up development environment
3. Begin Phase 1: Build System
4. Test on multiple platforms early
5. Iterate through phases