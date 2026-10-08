# Oculus

**Web-based smart sensor for shoulder range of motion (ROM) analysis on embedded Linux devices.**

> "Connect to your Oculus device, open the browser, and start measuring shoulder range of motion in real-time. No cloud, no complexity, no vendor lock-in."

**Hardware-agnostic.** Runs on any Linux ARM64/x86 device with automatic hardware detection.

---

## Features

### 🌐 Web-Based Interface
- Access via browser: `http://oculus.local` or `http://<device-ip>`
- No client software needed
- Responsive design (desktop, tablet, mobile)

### 📊 Real-time Pose Estimation
- Human pose estimation using RTMPose
- 17 keypoints detection
- Multi-person support
- Pose skeleton overlay on camera feed

### 📐 Joint Angle Measurement
- 10 joint angles tracked
- Real-time angle display
- Angle smoothing

### 📐 Shoulder ROM Analysis

#### Analysis Modes
- **Real-time**: Live camera feed with instant shoulder angle feedback
- **Record**: Record sessions for later analysis
- **Replay**: Playback recorded sessions with frame-by-frame control
- **Import**: Analyze existing video files (MP4, AVI, MOV)

#### Marker Support
- **Markerless** (default): AI-based pose estimation, no markers needed
- **ArUco Markers**: Optional markers for higher accuracy (±1-2cm)
- **Hybrid**: Auto-detect markers with AI fallback

#### Calibration
- **Auto**: No user input, estimates from body proportions (±10-15%)
- **Quick**: Stand still 3 seconds for automatic calibration (±5-10%)
- **Standard**: Enter height for scale reference (±3-5%)
- **Full**: Use ArUco board for maximum precision (±1-2%)

#### Shoulder ROM Metrics (11 Movements)
- **Elevation**: Abduction, Forward Flexion, Extension
- **Rotation**: External Rotation, Internal Rotation
- **Horizontal**: Adduction, Horizontal Adduction
- **Scapular**: Protraction, Retraction
- **Elevation/Depression**: Shoulder Shrug, Shoulder Depression
- Active ROM (degrees) per movement
- ROM symmetry (left vs right)
- Progress tracking over sessions

#### Equipment Requirements
- **Minimal**: Camera + Oculus device (markerless, auto calibration)
- **Basic**: + Tripod (~$20-50)
- **Standard**: + ArUco board, height reference (~$100-200)
- **Professional**: + 2 cameras, reflective markers (~$1000+)

### 🖥️ Multi-Platform Support
- **Automatic hardware detection** at startup
- **Auto-select best inference backend**
- Same binary runs on all platforms
- No recompilation needed

### 🛡️ Field Deployment Ready
- Watchdog auto-recovery
- Health monitoring
- Network resilience
- SQLite durability

---

## Supported Platforms

| Platform | Board Examples | Inference Backend | Status |
|----------|---------------|-------------------|--------|
| NVIDIA Jetson | Orin Nano, Xavier NX, AGX | TensorRT / CUDA | ✅ Primary |
| Rockchip RK3588 | Orange Pi 5, Radxa Rock 5 | RKNN | ✅ Supported |
| Raspberry Pi 5 | Pi 5 + AI Kit | Hailo / CPU | ✅ Supported |
| Intel x86 | NUC, Mini PC | OpenVINO / CPU | ✅ Supported |
| Generic ARM64 | Any ARM SBC | CPU | ✅ Basic |
| Generic x86 | Any x86 PC | CPU | ✅ Basic |

**The same Oculus binary runs on all platforms.** Hardware detection happens at runtime.

---

## Quick Start

### 1. Local Demo (No Hardware Required)

Test the shoulder ROM pipeline on your dev machine using a video file:

```bash
# Build with demo mode
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_DEMO=ON
cmake --build .

# Run demo with test video (git-ignored, local only)
./oculus-demo --video ../test-data/shoulder-rom/active-rom.mp4

# Open browser
open http://localhost:8080
```

See [Test Data Setup](test-data/shoulder-rom/README.md) for video file setup.

### 2. Build for Target

```bash
# Build for current platform
mkdir build && cd build
cmake ..
cmake --build .

# Or cross-compile for ARM64
./scripts/build.sh linux-arm64
```

### 3. Deploy

```bash
# Copy to target device
scp build/oculus user@<device-ip>:/usr/local/bin/

# Or use deploy script
./scripts/deploy.sh <device-ip>
```

### 4. Run

```bash
# On target device
./oculus
```

**Output:**
```
=== Oculus v0.1.0 ===
Platform: linux aarch64
CPU: ARM Cortex-A76 (8 cores)
Memory: 8192 MB (6144 MB available)
GPU: NVIDIA Orin (8192 MB VRAM)

Detecting inference backends...
  [✓] TensorRT EP - available
  [✓] CUDA EP - available
  [✗] OpenVINO EP - not available
  [✓] CPU EP - available

Selected backend: TensorRT
GPU acceleration: enabled

=== Oculus Ready ===
Web UI: http://oculus.local
API: http://oculus.local:8080
```

### 5. Access

Open browser and navigate to:
- `http://oculus.local` (mDNS)
- `http://<device-ip>`

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Web Browser                          │
│  Web UI + WebSocket (real-time updates)                 │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                Oculus Application                       │
│  ├── Hardware Detector (auto-detect)                    │
│  ├── ROM Analyzer (camera-agnostic)                     │
│  ├── Session Manager                                    │
│  └── Web Server                                         │
├─────────────────────────────────────────────────────────┤
│  Input Sources (separate implementations)               │
│  ├── FileCamera (test videos, dev/QA)                   │
│  ├── V4L2Camera (USB webcam, dev/demo)                  │
│  └── OrbbecCamera (stereo, production: RGB + Depth)     │
├─────────────────────────────────────────────────────────┤
│  Framework Layer                                        │
│  ├── Inference (ONNX Runtime + auto EP)                 │
│  ├── Storage (SQLite)                                   │
│  └── Watchdog + Health Monitor                          │
├─────────────────────────────────────────────────────────┤
│  Platform Layer                                         │
│  ├── Linux (ARM64 / x86_64)                             │
│  └── ONNX Runtime                                       │
│      ├── CPU EP (fallback)                              │
│      ├── TensorRT EP (NVIDIA, auto-detected)            │
│      ├── CUDA EP (NVIDIA, auto-detected)                │
│      └── OpenVINO EP (Intel, auto-detected)             │
└─────────────────────────────────────────────────────────┘
```

---

## Web Interface

### Pages

| Page | URL | Description |
|------|-----|-------------|
| Dashboard | `/` | Overview, quick stats, hardware info |
| Live View | `/live` | Camera feed + pose overlay + angles |
| Analysis | `/analysis` | Shoulder ROM analysis, charts |
| History | `/history` | Past sessions, replay |
| Settings | `/settings` | Configuration |
| System | `/system` | Logs, metrics, diagnostics |

### API

```yaml
GET  /api/v1/health              # Health check
GET  /api/v1/system/info         # System information
GET  /api/v1/system/hardware     # Hardware detection results
GET  /api/v1/status              # Runtime status
GET  /api/v1/metrics             # Performance metrics
GET  /api/v1/camera/stream       # MJPEG stream
GET  /api/v1/pose/current        # Current pose
GET  /api/v1/rom/angles          # Joint angles
GET  /api/v1/rom/metrics         # Shoulder ROM metrics
GET  /api/v1/rom/analysis        # ROM analysis results
GET  /api/v1/sessions            # List sessions
POST /api/v1/sessions            # Create session
GET  /api/v1/config              # Get config
PUT  /api/v1/config              # Update config
```

### WebSocket

```
ws://<device-ip>/ws/pose     # Real-time pose data
ws://<device-ip>/ws/angles   # Real-time joint angles
ws://<device-ip>/ws/metrics  # Real-time metrics
ws://<device-ip>/ws/rom      # Real-time ROM updates
```

---

## Configuration

Default configuration (`configs/default.yaml`):

```yaml
camera:
  device: "auto"  # auto, orbbec, v4l2
  resolution: "640x480"
  fps: 30

inference:
  model_path: "models/rtmpose.onnx"
  backend: "auto"  # auto, tensorrt, cuda, openvino, cpu
  precision: "fp16"

rom:
  joints_to_track:
    - left_shoulder
    - right_shoulder
  analysis_type: "active_rom"  # active_rom, passive_rom
  angle_smoothing: "moving_average"

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

All configuration accessible via web UI at `/settings`.

---

## Performance

Performance depends on hardware and selected backend:

| Backend | Target Latency | Power | Notes |
|---------|---------------|-------|-------|
| TensorRT FP16 | <15ms | Medium | Best for NVIDIA |
| CUDA | <25ms | Medium | Good for NVIDIA |
| OpenVINO | <20ms | Low-Medium | Best for Intel |
| RKNN | <25ms | Low | Best for Rockchip |
| CPU | <100ms | Low | Always works |

**Auto-selected at startup.** No manual configuration needed.

---

## Testing

TDD (Test-Driven Development) is mandatory.

```bash
# Run all tests
./scripts/run_tests.sh

# Run specific tests
cd build
ctest -R "unit/" --output-on-failure
ctest -R "integration/" --output-on-failure
```

---

## Field Deployment

### Checklist

- [ ] Enclosure (IP65+, ventilated)
- [ ] Cooling (heatsink + fan)
- [ ] Power (UPS, conditioner)
- [ ] Camera (weatherproof)
- [ ] Network (Ethernet primary, WiFi backup)
- [ ] Watchdog enabled
- [ ] Health monitoring active
- [ ] 24-hour stability test passed

### Systemd Service

```bash
sudo systemctl enable oculus
sudo systemctl start oculus
sudo systemctl status oculus
```

---

## Development

### Prerequisites

- CMake 3.15+
- C++20 compiler
- ONNX Runtime

### Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
```

### Test

```bash
cmake .. -DBUILD_TESTS=ON
cmake --build .
ctest --output-on-failure
```

---

## Documentation

- [Specification](PROMPT_SPEC.md)
- [Implementation Plan](IMPLEMENTATION_PLAN.md)
- [Testing Strategy](TESTING_STRATEGY.md)
- [Code of Conduct](CODE_OF_CONDUCT.md)

---

## Roadmap

| Version | Feature | Status |
|---------|---------|--------|
| v0.1 (MVP) | Shoulder Active ROM Analysis | 🔄 In Progress |
| v0.2 | Gait Analysis (gait cycle, step metrics, walking speed) | 📋 Planned |
| v0.3 | Advanced calibration & multi-camera | 📋 Planned |

---

## License

See [LICENSE](LICENSE)

---

## Target Users

- Physical therapists
- Sports scientists
- Biomechanics researchers
- Fitness coaches
- Healthcare providers

## Use Cases

- Shoulder ROM assessment (pre/post surgery)
- Sports performance analysis
- Rehabilitation monitoring
- Physical therapy progress tracking
- Movement research