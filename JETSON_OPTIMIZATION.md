# Oculus — Jetson Optimization & Field Deployment

## Jetson Accelerators Overview

### 1. JetPack SDK (Automatic GPU Management)

NVIDIA Jetson uses **JetPack SDK** which manages GPU automatically:

```
┌─────────────────────────────────────────────────────────┐
│                    JetPack SDK                          │
├─────────────────────────────────────────────────────────┤
│  CUDA Runtime      ← Automatic GPU memory management  │
│  cuDNN             ← Deep learning primitives          │
│  TensorRT          ← Model optimization (auto)         │
│  NVDEC/NVENC       ← Hardware video decode/encode      │
│  VPI               ← Vision Programming Interface      │
│  DLA               ← Deep Learning Accelerator         │
└─────────────────────────────────────────────────────────┘
```

**What Oculus needs to do:**
- Use ONNX Runtime with TensorRT Execution Provider
- No need to control GPU memory manually
- No need to manage CUDA streams manually
- JetPack handles everything

---

### 2. TensorRT (Automatic Model Optimization)

TensorRT optimizes the model automatically:

```
ONNX Model (RTMPose)
        ↓
    TensorRT Parser
        ↓
    Graph Optimization
    ├── Layer fusion
    ├── Precision calibration (FP16/INT8)
    ├── Kernel auto-tuning
    └── Memory optimization
        ↓
    TensorRT Engine (.engine)
        ↓
    Optimized Inference (2-5x faster)
```

**Benefits:**
- No need to hand-tune GPU code
- Auto-select best precision (FP32/FP16/INT8)
- Auto-optimize for specific Jetson model
- Cache engine for faster startup

---

### 3. Deep Learning Accelerator (DLA)

Jetson Orin Nano has **DLA cores** for inference:

```
┌─────────────────────────────────────────────────────────┐
│              Jetson Orin Nano 8GB                       │
├─────────────────────────────────────────────────────────┤
│  GPU (Ampere)      ← 1024 CUDA cores                    │
│                      32 Tensor cores                    │
│  DLA Core          ← Dedicated inference accelerator    │
│                      Offload from GPU                   │
│  CPU (Carmel)      ← 6-core ARM                        │
│  Memory            ← 8GB LPDDR5                         │
└─────────────────────────────────────────────────────────┘
```

**DLA Benefits:**
- Dedicated inference accelerator
- Does not compete with GPU tasks
- Lower power consumption
- Better thermal performance

---

## Performance Optimization Strategy

### 4. Automatic Optimization Pipeline

```
Step 1: ONNX Model (RTMPose)
            ↓
Step 2: TensorRT Optimization
        ├── Auto kernel selection
        ├── Auto precision (FP16)
        ├── Auto layer fusion
        └── Auto memory planning
            ↓
Step 3: Engine Cache (.engine file)
            ↓
Step 4: Runtime Inference
        ├── GPU (default)
        ├── DLA (optional offload)
        └── CPU (fallback)
```

### 5. ONNX Runtime Execution Providers

```cpp
// Oculus inference configuration
Ort::SessionOptions session_options;

// Option 1: TensorRT (fastest, recommended)
session_options.AppendExecutionProvider_TensorRT(
    OrtTensorRTProviderOptions{
        .device_id = 0,
        .trt_max_workspace_size = 1ULL << 30,  // 1GB
        .trt_fp16_enable = 1,                   // Auto FP16
        .trt_engine_cache_enable = 1,           // Cache engine
        .trt_engine_cache_path = "/tmp/trt_cache"
    }
);

// Option 2: CUDA (good fallback)
session_options.AppendExecutionProvider_CUDA(
    OrtCUDAProviderOptions{
        .device_id = 0,
        .cudnn_conv_algo_search = OrtCudnnConvAlgoSearchDefault
    }
);

// Option 3: CPU (always works)
// Default, no need to append
```

### 6. Performance Expectations

| Backend | RTMPose (256x192) | Power | Use Case |
|---------|-------------------|-------|----------|
| CPU | ~100ms | 5W | Fallback only |
| CUDA | ~25ms | 15W | Development |
| TensorRT FP32 | ~15ms | 15W | High accuracy |
| TensorRT FP16 | ~10ms | 12W | **Recommended** |
| TensorRT INT8 | ~7ms | 10W | Max performance |
| DLA FP16 | ~12ms | 8W | Low power |

**Target: 30+ FPS with TensorRT FP16**

---

## Field Deployment — Durability & Reliability

### 7. Thermal Management

**Problem:** Jetson throttles when temperature exceeds 85°C

**Solution:**
```cpp
class ThermalManager {
public:
    // Monitor temperature
    float get_cpu_temp();
    float get_gpu_temp();
    float get_board_temp();
    
    // Thermal policies
    enum class ThermalPolicy {
        PERFORMANCE,    // Max performance, allow higher temp
        BALANCED,       // Balance performance/thermal
        POWER_SAVE,     // Reduce performance to stay cool
        EMERGENCY       // Throttle hard, prevent shutdown
    };
    
    void set_policy(ThermalPolicy policy);
    
    // Auto-adjust
    void enable_auto_throttle(bool enable);
};
```

**Hardware Recommendations:**
- Heatsink + fan (active cooling)
- Thermal paste/pad
- Ventilated enclosure
- Ambient temp <40°C

---

### 8. Power Management

**Problem:** Power instability in field deployment

**Solution:**
```cpp
class PowerManager {
public:
    // Power modes (Jetson presets)
    enum class PowerMode {
        MAXN,       // 15W, max performance
        MODE_15W,   // 15W, balanced
        MODE_10W,   // 10W, power save
        MODE_7W     // 7W, minimal
    };
    
    void set_power_mode(PowerMode mode);
    
    // Monitor
    float get_power_consumption();  // watts
    float get_voltage();
    float get_current();
    
    // Battery backup (if applicable)
    float get_battery_level();
    bool is_on_battery();
    void set_low_battery_action(Action action);
};
```

**Hardware Recommendations:**
- UPS/battery backup
- Power conditioner
- Surge protector
- Redundant power supply
- Power monitoring

---

### 9. Watchdog & Auto-Recovery

**Problem:** Application crashes in field, no one to restart

**Solution:**
```cpp
class WatchdogService {
public:
    // System watchdog
    void enable_system_watchdog(int timeout_seconds = 30);
    void feed();  // Reset watchdog timer
    
    // Application watchdog
    void enable_app_watchdog(int timeout_seconds = 10);
    void heartbeat();
    
    // Recovery actions
    enum class RecoveryAction {
        RESTART_APP,        // Restart Oculus app
        RESTART_SERVICE,    // Restart systemd service
        REBOOT_SYSTEM,      // Full reboot
        ALERT_ONLY          // Just log/alert
    };
    
    void set_recovery_action(RecoveryAction action);
};
```

**Systemd Service:**
```ini
# /etc/systemd/system/oculus.service
[Unit]
Description=Oculus Smart Sensor
After=network.target
StartLimitIntervalSec=300
StartLimitBurst=5

[Service]
Type=simple
ExecStart=/usr/local/bin/oculus
Restart=always
RestartSec=5
WatchdogSec=30
TimeoutStartSec=30
TimeoutStopSec=10

# Resource limits
LimitNOFILE=65536
LimitNPROC=4096

# Security
User=oculus
Group=oculus
NoNewPrivileges=true
ProtectSystem=strict
ReadWritePaths=/data/oculus

[Install]
WantedBy=multi-user.target
```

---

### 10. Health Monitoring

**Continuous Health Checks:**
```cpp
class HealthMonitor {
public:
    struct HealthStatus {
        // System
        float cpu_usage;
        float memory_usage;
        float disk_usage;
        float cpu_temp;
        float gpu_temp;
        
        // Application
        bool camera_connected;
        bool inference_ready;
        bool storage_healthy;
        bool http_responsive;
        
        // Performance
        double fps;
        double latency_ms;
        int dropped_frames;
        int inference_errors;
        
        // Uptime
        std::chrono::seconds uptime;
        int restart_count;
    };
    
    HealthStatus check_health();
    
    // Alerts
    void set_threshold(string metric, float warning, float critical);
    vector<Alert> get_active_alerts();
    
    // Auto-healing
    void enable_auto_healing(bool enable);
    void set_healing_strategy(string metric, HealingStrategy strategy);
};
```

---

### 11. Data Persistence & Recovery

**Problem:** Power loss, data corruption

**Solution:**
```cpp
class ResilientStorage {
public:
    // Write-ahead logging
    void enable_wal(bool enable);
    
    // Auto-backup
    void enable_auto_backup(
        std::chrono::minutes interval,
        string backup_path
    );
    
    // Corruption recovery
    bool verify_database();
    bool repair_database();
    bool restore_from_backup();
    
    // Sync to external storage
    void enable_cloud_sync(string endpoint);
    void enable_usb_sync(string mount_point);
};
```

**SQLite Durability:**
```cpp
// Connection with durability options
sqlite3_exec(db, "PRAGMA journal_mode=WAL;", ...);      // Write-ahead logging
sqlite3_exec(db, "PRAGMA synchronous=FULL;", ...);       // Full sync
sqlite3_exec(db, "PRAGMA temp_store=MEMORY;", ...);      // Temp in memory
sqlite3_exec(db, "PRAGMA mmap_size=268435456;", ...);    // 256MB mmap
```

---

### 12. Network Resilience

**Problem:** Unstable network in field deployment

**Solution:**
```cpp
class NetworkManager {
public:
    // Connection modes
    enum class NetworkMode {
        ETHERNET,       // Wired (most reliable)
        WIFI,           // Wireless
        CELLULAR,       // 4G/5G backup
        OFFLINE         // Local only
    };
    
    // Auto-failover
    void enable_failover(vector<NetworkMode> priority);
    
    // Offline mode
    void enable_offline_mode(bool enable);
    void queue_remote_sync(Data data);
    void sync_when_online();
    
    // mDNS (local discovery)
    void enable_mdns(string hostname);  // oculus.local
    
    // Connection health
    bool is_connected();
    float get_signal_strength();
    float get_latency();
};
```

---

### 13. Logging & Diagnostics

**Field Diagnostics:**
```cpp
class DiagnosticsManager {
public:
    // Structured logging (persistent)
    void enable_file_logging(string path, size_t max_size);
    void enable_remote_logging(string endpoint);
    
    // Log rotation
    void set_rotation_policy(
        size_t max_size_mb,
        int max_files,
        std::chrono::days max_age
    );
    
    // Crash dumps
    void enable_crash_dumps(string path);
    void upload_crash_dumps(string endpoint);
    
    // Performance profiling
    void enable_profiling(bool enable);
    string generate_performance_report();
    
    // Remote diagnostics
    void enable_remote_shell(bool enable);  // SSH
    void enable_remote_api(bool enable);    // Debug API
};
```

---

### 14. Update & Maintenance

**OTA Update (Future):**
```cpp
class UpdateManager {
public:
    // Check for updates
    bool check_for_updates();
    UpdateInfo get_update_info();
    
    // Apply update
    bool download_update(string url);
    bool apply_update(string path);
    bool rollback_update();
    
    // Update modes
    enum class UpdateMode {
        AUTOMATIC,      // Auto-download, auto-apply
        NOTIFY_ONLY,    // Notify user
        MANUAL          // User-initiated only
    };
    
    void set_update_mode(UpdateMode mode);
};
```

---

## Field Deployment Checklist

### Hardware

- [ ] **Enclosure**
  - IP65+ rated (dust/water resistant)
  - Ventilated (active cooling)
  - Vibration resistant
  - UV resistant (outdoor)

- [ ] **Power**
  - UPS/battery backup
  - Power conditioner
  - Surge protector
  - Redundant supply (optional)

- [ ] **Connectivity**
  - Ethernet (primary)
  - WiFi (backup)
  - Cellular (optional)
  - Antenna externalized

- [ ] **Camera**
  - Weatherproof housing
  - IR illumination (low light)
  - Anti-fog coating
  - Secure mounting

### Software

- [ ] **System**
  - Watchdog enabled
  - Auto-restart on crash
  - Health monitoring
  - Remote access (SSH/API)

- [ ] **Storage**
  - WAL mode enabled
  - Auto-backup configured
  - Log rotation enabled
  - Crash dumps captured

- [ ] **Network**
  - mDNS enabled
  - Offline mode capable
  - Auto-reconnect
  - Failover configured

- [ ] **Security**
  - Non-root user
  - Firewall configured
  - API authentication
  - Encrypted storage (optional)

---

## Performance Targets

| Metric | Target | Measurement |
|--------|--------|-------------|
| Inference Latency | <15ms | TensorRT FP16 |
| FPS | ≥30 | Stable frame rate |
| Startup Time | <10s | Camera ready |
| Memory Usage | <2GB | Stable over time |
| CPU Usage | <50% | With GPU offload |
| Power Consumption | <15W | Typical workload |
| Operating Temp | 0-50°C | Ambient |
| Uptime | 99.9% | With watchdog |
| Recovery Time | <30s | After crash |

---

## Summary

### How to Use Jetson Accelerators (No Manual Control Needed)

```
1. ONNX Runtime + TensorRT EP
   → TensorRT optimizes model automatically
   → Auto precision (FP16/INT8)
   → Auto kernel selection
   → Cache engine for faster restart

2. CUDA Runtime
   → Automatic memory management
   → Automatic stream management
   → No need to call cudaMalloc/cudaFree

3. DLA (Deep Learning Accelerator)
   → Dedicated inference hardware
   → Offloads from GPU
   → Lower power consumption

4. JetPack Power Management
   → Auto power mode selection
   → Automatic thermal throttling
   → DVFS (Dynamic Voltage Frequency Scaling)
```

### How to Make It Durable (Field Deployment)

```
1. Watchdog + Auto-recovery
   → Systemd watchdog
   → Application watchdog
   → Auto-restart on crash

2. Health Monitoring
   → Continuous health checks
   → Alert on anomalies
   → Auto-healing

3. Thermal Management
   → Monitor temperature
   → Auto-throttle on overheat
   → Active cooling

4. Power Resilience
   → UPS/battery backup
   → Graceful shutdown
   → Power monitoring

5. Data Durability
   → SQLite WAL mode
   → Auto-backup
   → Corruption recovery

6. Network Resilience
   → Offline mode
   → Auto-reconnect
   → Failover
```

---

## Next Steps

1. **Implement TensorRT EP** in ONNX Runtime integration
2. **Add watchdog service** for auto-recovery
3. **Implement health monitoring** for field diagnostics
4. **Add thermal management** for temperature control
5. **Configure systemd service** for auto-start

Which section should be implemented first?