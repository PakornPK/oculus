# Testing Strategy for Oculus

## Overview

Oculus uses a layered testing approach that allows comprehensive testing without requiring hardware (camera, GPU).

## Test Types

### 1. Unit Tests (No Hardware Required)

Tests component logic without requiring camera or GPU connection:

```
tests/unit/
├── core/
│   ├── frame_test.cpp          ← Frame creation, manipulation
│   ├── pose_test.cpp           ← Keypoint/Pose data structures
│   └── config_test.cpp         ← Config parsing, validation, defaults
├── camera/
│   └── mock_camera_test.cpp    ← Mock camera for pipeline testing
├── inference/
│   ├── preprocessor_test.cpp   ← Image preprocessing logic
│   └── postprocessor_test.cpp  ← Keypoint extraction logic
├── storage/
│   └── sqlite_storage_test.cpp ← CRUD operations, migrations
└── http/
    └── controller_test.cpp     ← API responses, status codes
```

**What to test:**
- Frame creation and manipulation
- Pose data structures
- Configuration parsing and validation
- RTMPose preprocessor logic (resize, normalize, format conversion)
- RTMPose postprocessor logic (heatmap → keypoints)
- SQLite storage operations
- HTTP controller responses

### 2. Integration Tests (Using Mock Components)

Tests the full pipeline with mock components:

```
tests/integration/
└── pipeline_test.cpp
```

**What to test:**
- Full pipeline: Camera → Inference → Result → Storage
- Bounded queue behavior (prevents memory growth)
- Error handling (camera failure, inference failure)
- Performance metrics collection
- Concurrent access

### 3. Hardware Tests (Real Hardware Required)

Tests with real hardware:

```
tests/hardware/
├── astra_pro_test.cpp          ← Orbbec Astra Pro camera
├── onnx_runtime_test.cpp       ← ONNX Runtime with real models
└── performance_benchmark.cpp   ← Performance benchmarks
```

**What to test:**
- Camera initialization and frame capture
- ONNX Runtime inference with real models
- Performance benchmarks (latency, FPS)

## Mock Objects

Uses mock objects for testing without hardware:

### MockCamera
- Simulates camera behavior
- Inject test frames
- Simulate failures (fail on open)
- Loop frames for continuous testing

### MockInferenceEngine
- Simulates inference behavior
- Inject test pose results
- Custom result generators
- Track inference count

### MockStorage
- Simulates storage behavior
- Track store operations
- Simulate database errors

## Running Tests

### Prerequisites

```bash
# macOS
brew install googletest

# Ubuntu/Debian
sudo apt-get install libgtest-dev

# Cross-compilation for Linux ARM64
# Use Docker-based build environment
```

### Build and Run Tests

```bash
# Method 1: Using script
./scripts/run_tests.sh

# Method 2: Manual
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build .
ctest --output-on-failure
```

### Run Specific Tests

```bash
# Run only unit tests
ctest -R "unit/" --output-on-failure

# Run only integration tests
ctest -R "integration/" --output-on-failure

# Run specific test
ctest -R "preprocessor_test" --output-on-failure
```

## Test Coverage

### Critical Paths (Must Test)

1. **Frame Pipeline**
   - Frame creation
   - Frame queue (bounded)
   - Frame capture thread

2. **Inference Pipeline**
   - Model loading
   - Preprocessing (resize, normalize)
   - Inference execution
   - Postprocessing (heatmap → keypoints)

3. **Storage**
   - Database initialization
   - Event storage
   - Metrics storage
   - Configuration storage
   - Migrations

4. **HTTP API**
   - Health endpoint
   - System info endpoint
   - Metrics endpoint
   - Pose endpoint

5. **Shoulder ROM Analysis**
   - Joint angle calculation (shoulder flexion/extension/abduction/adduction)
   - ROM calculation (min/max/ROM degrees)
   - Left vs right comparison (symmetry %)
   - Angle smoothing (moving average, Kalman filter)
   - Camera angle compensation

5. **Error Handling**
   - Camera failures
   - Inference failures
   - Storage failures
   - Configuration errors

### Test Examples

#### Preprocessor Test
```cpp
TEST(RTMPosePreprocessorTest, ResizeToModelInputSize) {
    Frame frame = create_test_frame(640, 480);
    RTMPosePreprocessor preprocessor(256, 192);
    
    auto input_tensor = preprocessor.process(frame);
    
    EXPECT_EQ(input_tensor.shape[2], 192);  // height
    EXPECT_EQ(input_tensor.shape[3], 256);  // width
}
```

#### Postprocessor Test
```cpp
TEST_F(RTMPosePostprocessorTest, ExtractsKeypointsFromHeatmap) {
    RTMPosePostprocessor postprocessor(256, 192, 48, 64, 17);
    
    auto poses = postprocessor.process(mock_heatmap);
    
    ASSERT_EQ(poses.size(), 1);
    EXPECT_EQ(poses[0].keypoints.size(), 17);
    EXPECT_GT(poses[0].keypoints[0].confidence, 0.9f);
}
```

#### Pipeline Integration Test
```cpp
TEST_F(PipelineIntegrationTest, ProcessesFramesEndToEnd) {
    mock_camera->enqueue_frame(create_test_frame());
    mock_inference->enqueue_result(create_test_pose_result());
    
    ASSERT_TRUE(pipeline->start());
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto results = pipeline->get_latest_results(3);
    EXPECT_GE(results.size(), 1);
    EXPECT_FALSE(results[0].poses.empty());
}
```

#### Shoulder ROM Analyzer Test
```cpp
TEST(ShoulderROMAnalyzerTest, CalculatesFlexionAngle) {
    ShoulderROMAnalyzer analyzer;
    
    // Create pose with known shoulder position
    Pose pose;
    pose.keypoints[LEFT_ELBOW] = {100, 300, 0.95f};
    pose.keypoints[LEFT_SHOULDER] = {150, 200, 0.98f};
    pose.keypoints[LEFT_HIP] = {160, 350, 0.97f};
    
    float angle = analyzer.calculate_flexion(pose, Side::LEFT);
    
    // Expected: ~90 degrees (rough estimate from coordinates)
    EXPECT_NEAR(angle, 90.0f, 10.0f);
}

TEST(ShoulderROMAnalyzerTest, TracksMinMaxROM) {
    ShoulderROMAnalyzer analyzer;
    
    // Simulate shoulder movement
    analyzer.update(30.0f);  // Starting position
    analyzer.update(45.0f);  // Moving up
    analyzer.update(90.0f);  // Max flexion
    analyzer.update(60.0f);  // Coming down
    
    auto rom = analyzer.get_rom();
    EXPECT_FLOAT_EQ(rom.min_angle, 30.0f);
    EXPECT_FLOAT_EQ(rom.max_angle, 90.0f);
    EXPECT_FLOAT_EQ(rom.rom_degrees, 60.0f);
}

TEST(ShoulderROMAnalyzerTest, CalculatesSymmetry) {
    ShoulderROMAnalyzer analyzer;
    
    // Left shoulder: 90° ROM
    analyzer.update_left(10.0f);
    analyzer.update_left(100.0f);
    
    // Right shoulder: 80° ROM (slightly less)
    analyzer.update_right(15.0f);
    analyzer.update_right(95.0f);
    
    float symmetry = analyzer.get_symmetry_percent();
    // 80/90 = 88.9%
    EXPECT_NEAR(symmetry, 88.9f, 1.0f);
}
```

## Test Configuration

### CMake Configuration

```cmake
# Enable tests
option(BUILD_TESTS "Build tests" ON)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

### Test Environment

- **Debug builds**: Include debug symbols, assertions
- **Release builds**: Optimized, no debug assertions
- **Cross-compilation**: Use Docker for Linux ARM64 tests

## Continuous Integration

### GitHub Actions Example

```yaml
name: Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y libgtest-dev cmake
      - name: Build and test
        run: |
          mkdir build && cd build
          cmake .. -DBUILD_TESTS=ON
          cmake --build .
          ctest --output-on-failure
```

## Best Practices

1. **Test Early, Test Often**
   - Run tests before committing
   - Run tests after refactoring
   - Run tests in CI/CD

2. **Isolate Tests**
   - Each test should be independent
   - Use setup/teardown for clean state
   - Don't rely on test execution order

3. **Use Mocks Wisely**
   - Mock external dependencies (hardware, network)
   - Don't mock everything (test real logic)
   - Keep mocks simple

4. **Test Edge Cases**
   - Empty inputs
   - Invalid inputs
   - Boundary conditions
   - Error conditions

5. **Measure Coverage**
   - Aim for >80% code coverage
   - Focus on critical paths
   - Don't chase 100% coverage

## Troubleshooting

### Common Issues

1. **Tests fail to compile**
   - Check include paths
   - Verify library dependencies
   - Check C++ standard (C++20)

2. **Tests crash**
   - Check for null pointers
   - Verify memory management
   - Check thread safety

3. **Integration tests slow**
   - Reduce test data size
   - Use faster mock implementations
   - Parallelize tests

4. **Hardware tests fail**
   - Verify hardware connection
   - Check permissions
   - Verify driver installation

## Resources

- [Google Test Documentation](https://google.github.io/googletest/)
- [CMake Testing](https://cmake.org/cmake/help/latest/command/ctest.html)
- [Test-Driven Development](https://en.wikipedia.org/wiki/Test-driven_development)
