# Oculus — AI Coding Agent Code of Conduct

## 1. Primary Objective

Build Oculus as a small, maintainable, production-oriented embedded C++ framework.

The goal is not to maximize features.

The goal is:

> Make the smallest correct implementation that establishes a strong foundation for the Oculus product.

---

## 2. Do Not Overengineer

Prefer simple solutions.

Before introducing a new abstraction, framework, dependency, thread, service, or design pattern, ask:

> Is this required by the current problem?

If the answer is no, do not add it.

Avoid:

* unnecessary factories
* unnecessary dependency injection frameworks
* excessive interfaces
* generic abstractions without a real use case
* microservices
* distributed architecture
* unnecessary template metaprogramming
* complex event buses

Abstraction must solve a real problem.

---

## 3. Hardware Independence

Never allow NVIDIA-specific APIs to leak into the application layer.

Bad:

```cpp
cudaMalloc(...);
cudaMemcpy(...);
```

inside pose/business logic.

Good:

```cpp
inferenceEngine->infer(frame);
```

Hardware acceleration belongs inside an inference backend.

---

## 4. ONNX Is the Contract

ONNX is the primary model contract.

Do not design the application around:

* CUDA
* TensorRT
* RKNN
* vendor-specific inference APIs

Any vendor backend must be replaceable.

---

## 5. Dependency Discipline

Before adding a dependency:

1. Check whether the standard library is sufficient.
2. Check whether an existing project dependency can solve it.
3. Check binary size.
4. Check license.
5. Check ARM64/Linux compatibility.
6. Check long-term maintenance cost.

Do not add dependencies simply because they are popular.

---

## 6. Single Binary Is a Product Requirement

The production runtime should ultimately be:

```text
oculus
```

Do not create a deployment architecture requiring:

```text
oculus-core.so
oculus-camera.so
oculus-inference.so
...
```

unless there is a concrete technical requirement.

Internal modularity is good.

Deployment fragmentation is not.

---

## 7. Product Versioning

Think in terms of product releases.

Prefer:

```text
Oculus 1.2.0
```

over requiring users to understand a dependency matrix.

Internal versions and hashes are allowed for diagnostics.

Example:

```text
product: Oculus 1.2.0
commit: abc123
model: rtmpose
model_hash: ...
backend: onnxruntime-cuda
```

But the support conversation should primarily use:

```text
Oculus 1.2.0
```

---

## 8. Performance

Never claim performance without measurement.

Do not say:

> "This should be fast."

Measure:

* FPS
* latency
* CPU
* memory
* startup time
* queue depth
* thermal behavior where available

Use benchmarks before optimization.

---

## 9. Optimize Only After Profiling

Do not prematurely introduce:

* CUDA kernels
* custom allocators
* lock-free structures
* SIMD
* complicated thread pools
* TensorRT-specific code

unless profiling demonstrates a real bottleneck.

---

## 10. Memory Safety

Modern C++ practices are mandatory.

Prefer:

```cpp
std::unique_ptr
std::shared_ptr
std::vector
std::string
std::span
std::jthread
std::chrono
```

Use RAII.

Avoid:

```cpp
new
delete
malloc
free
```

unless there is a justified low-level requirement.

Ownership must always be obvious.

---

## 11. Concurrency

Every thread must have:

* clear ownership
* clear shutdown behavior
* bounded resources

Prefer:

```cpp
std::jthread
std::stop_token
```

over manually managed thread lifecycles.

Never create an unbounded producer queue for camera frames.

When inference is slower than capture, define an explicit frame-dropping/backpressure policy.

For real-time vision, processing the newest frame is generally preferable to building a large latency queue.

---

## 12. Error Handling

Never hide errors.

Bad:

```cpp
try {
    ...
} catch (...) {}
```

Bad:

```cpp
if (!camera.open()) {
    return;
}
```

without logging or explaining why.

Good:

```text
ERROR camera initialization failed
device=astra-pro
reason=...
```

Errors should contain useful context.

---

## 13. Logging

Logs must help an engineer diagnose the system without attaching a debugger.

Startup should clearly report:

```text
Oculus 0.1.0
platform: linux-arm64
camera: Astra Pro
model: RTMPose
inference backend: ONNX Runtime / CUDA
resolution: 640x480
target FPS: 15
```

Do not spam logs inside high-frequency loops.

Never log secrets or sensitive user data.

---

## 14. Configuration

Use safe defaults.

A fresh installation should require minimal configuration.

Do not make users manually configure ten different files to start the product.

Configuration validation must happen during startup.

Invalid configuration should produce a clear error.

---

## 15. API Design

HTTP handlers should remain thin.

Bad:

```text
HTTP handler
    ├── SQL
    ├── inference
    ├── camera control
    └── business logic
```

Good:

```text
HTTP
  ↓
Controller
  ↓
Service
  ↓
Domain
```

Keep business logic independent of HTTP.

---

## 16. Database

SQL must not be scattered throughout the codebase.

Use a storage layer.

Always use parameterized queries.

Database schema changes must use migrations.

Never silently modify the schema at runtime without a controlled migration mechanism.

---

## 17. Test-Driven Development (TDD)

TDD is mandatory for all non-trivial code.

### 17.1 TDD Cycle

Follow the Red-Green-Refactor cycle:

```text
1. RED     → Write a failing test first
2. GREEN   → Write the minimum code to pass the test
3. REFACTOR → Improve code while keeping tests green
```

Never write production code without a failing test that requires it.

### 17.2 Test First Rules

Before implementing any feature:

1. Write the test that defines the expected behavior
2. Run the test and verify it fails (RED)
3. Implement the minimum code to make it pass (GREEN)
4. Refactor if needed while keeping tests green
5. Commit with test and implementation together

Bad:

```cpp
// 1. Write implementation
void Preprocessor::process(Frame& frame) {
    // ... 200 lines of code ...
}

// 2. Maybe write test later
TEST(PreprocessorTest, ProcessFrame) {
    // ... test after the fact ...
}
```

Good:

```cpp
// 1. Write test first
TEST(PreprocessorTest, ResizesFrameToModelInputSize) {
    Frame frame = create_test_frame(640, 480);
    Preprocessor preprocessor(256, 192);
    
    auto result = preprocessor.process(frame);
    
    EXPECT_EQ(result.width, 256);
    EXPECT_EQ(result.height, 192);
}

// 2. Run test → RED (fails, Preprocessor doesn't exist yet)

// 3. Implement minimum code
class Preprocessor {
public:
    Preprocessor(int width, int height) : width_(width), height_(height) {}
    
    ProcessedFrame process(const Frame& frame) {
        // Minimum implementation to pass test
    }
};

// 4. Run test → GREEN (passes)

// 5. Refactor if needed
```

### 17.3 Test Coverage Requirements

Minimum test coverage for each component:

| Component | Unit Tests | Integration Tests | Hardware Tests |
|-----------|------------|-------------------|----------------|
| Core (Frame, Pose, Config) | 100% | N/A | N/A |
| Preprocessor | 100% | Required | Optional |
| Postprocessor | 100% | Required | Optional |
| Storage | 100% | Required | N/A |
| HTTP Controllers | 100% | Required | N/A |
| Camera | Mock required | Required | Required |
| Inference Engine | Mock required | Required | Required |
| Pipeline | N/A | 100% | Required |

### 17.4 Test Categories

Separate tests into clear categories:

```text
tests/
├── unit/                    ← Pure logic, no hardware
│   ├── core/
│   ├── inference/
│   ├── storage/
│   └── http/
├── integration/             ← Mock components, pipeline tests
│   └── pipeline_test.cpp
└── hardware/                ← Real hardware required
    ├── camera_test.cpp
    ├── inference_test.cpp
    └── benchmark_test.cpp
```

### 17.5 Test Isolation

Every test must be independent:

* No shared state between tests
* No test execution order dependencies
* Clean setup and teardown for each test
* Use fixtures for common setup

Bad:

```cpp
static int shared_counter = 0;  // Shared state!

TEST(SuiteA, Test1) {
    shared_counter++;
    EXPECT_EQ(shared_counter, 1);
}

TEST(SuiteA, Test2) {
    shared_counter++;  // Depends on Test1 running first!
    EXPECT_EQ(shared_counter, 2);
}
```

Good:

```cpp
class CounterTest : public ::testing::Test {
protected:
    void SetUp() override {
        counter = 0;  // Fresh state for each test
    }
    int counter;
};

TEST_F(CounterTest, IncrementsFromZero) {
    counter++;
    EXPECT_EQ(counter, 1);
}

TEST_F(CounterTest, IncrementsFromZeroAgain) {
    counter++;  // Independent, doesn't depend on other test
    EXPECT_EQ(counter, 1);
}
```

### 17.6 Mock Strategy

Use mocks to isolate hardware dependencies:

```text
Real Component          Mock Replacement
──────────────          ────────────────
Orbbec Astra Pro   →    MockCamera
ONNX Runtime       →    MockInferenceEngine
SQLite             →    In-memory SQLite (or MockStorage)
HTTP Server        →    MockHttpServer
```

Mocks must:

* Simulate real behavior accurately
* Support failure injection
* Track call counts for verification
* Be reusable across tests

### 17.7 Test Naming Convention

Use descriptive test names that explain the behavior:

Bad:

```cpp
TEST(PreprocessorTest, Test1) { ... }
TEST(PreprocessorTest, Works) { ... }
```

Good:

```cpp
TEST(PreprocessorTest, ResizesFrameToModelInputSize) { ... }
TEST(PreprocessorTest, NormalizesPixelValuesToZeroOne) { ... }
TEST(PreprocessorTest, ThrowsOnEmptyFrame) { ... }
TEST(PreprocessorTest, HandlesGrayscaleInput) { ... }
```

Pattern: `[Component]_[Behavior]_[Condition]`

### 17.8 Assertion Best Practices

Use specific assertions:

Bad:

```cpp
EXPECT_TRUE(result.ok);  // What does "ok" mean?
```

Good:

```cpp
EXPECT_EQ(result.width, 256);
EXPECT_EQ(result.height, 192);
EXPECT_GT(result.keypoints.size(), 0);
EXPECT_NEAR(result.keypoints[0].confidence, 0.95f, 0.01f);
```

### 17.9 Test Data Management

Use helper functions for test data:

```cpp
// Good: Reusable test data creators
Frame create_test_frame(int width = 640, int height = 480);
PoseResult create_test_pose_result(int num_poses = 1);
Config create_default_config();

// Good: Named constants
constexpr int kDefaultWidth = 640;
constexpr int kDefaultHeight = 480;
constexpr float kConfidenceThreshold = 0.5f;
```

### 17.10 Continuous Integration

All tests must pass before merge:

* Unit tests: Run on every commit
* Integration tests: Run on every PR
* Hardware tests: Run on release branch (or nightly)

Never merge code with failing tests.

### 17.11 Test Performance

Tests should be fast:

* Unit tests: < 100ms each
* Integration tests: < 1s each
* Hardware tests: < 10s each (excluding warmup)

If a test is slow:

* Profile the test
* Reduce test data size
* Use mocks instead of real implementations
* Consider marking as `[SLOW]` and running separately

### 17.12 Regression Tests

When fixing a bug:

1. Write a test that reproduces the bug
2. Verify the test fails (RED)
3. Fix the bug
4. Verify the test passes (GREEN)
5. Keep the test forever

Never fix a bug without a regression test.

### 17.13 Test Documentation

Each test file should have a header comment:

```cpp
/**
 * @file preprocessor_test.cpp
 * @brief Unit tests for RTMPose preprocessor
 * 
 * Tests cover:
 * - Frame resizing to model input size
 * - Pixel normalization
 * - Color space conversion
 * - Error handling for invalid inputs
 */
```

---

## 18. Testing Infrastructure

### 18.1 Test Framework

Use Google Test (gtest) as the primary test framework:

```cmake
find_package(GTest REQUIRED)
target_link_libraries(tests PRIVATE GTest::gtest GTest::gtest_main)
```

### 18.2 Test Build Configuration

Tests must build separately from production:

```cmake
option(BUILD_TESTS "Build tests" ON)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

### 18.3 Test Execution

Provide convenient test execution:

```bash
# Run all tests
ctest --output-on-failure

# Run specific test category
ctest -R "unit/" --output-on-failure
ctest -R "integration/" --output-on-failure

# Run specific test
ctest -R "PreprocessorTest" --output-on-failure
```

### 18.4 Test Helpers

Create reusable test utilities:

```cpp
// tests/test_helpers.hpp
namespace oculus::test {

Frame create_test_frame(int width, int height);
PoseResult create_test_pose_result(int num_poses);
Config create_default_config();
void assert_pose_equal(const Pose& a, const Pose& b, float tolerance);

} // namespace oculus::test
```

---

## 19. Testing Anti-Patterns

Avoid these testing mistakes:

### 19.1 Don't Test Implementation Details

Bad:

```cpp
TEST(PreprocessorTest, CallsResizeFunction) {
    // Testing internal implementation
    MockImageProcessor mock;
    EXPECT_CALL(mock, resize(testing::_)).Times(1);
    // ...
}
```

Good:

```cpp
TEST(PreprocessorTest, ProducesCorrectOutputSize) {
    // Testing behavior
    Frame input = create_test_frame(640, 480);
    auto output = preprocessor.process(input);
    EXPECT_EQ(output.width, 256);
    EXPECT_EQ(output.height, 192);
}
```

### 19.2 Don't Write Tests After Implementation

Bad:

```cpp
// 1. Write 500 lines of code
// 2. "Oh, I should write tests"
// 3. Write tests that match implementation (not requirements)
```

Good:

```cpp
// 1. Write test defining requirement
// 2. Implement minimum code to pass
// 3. Refactor
```

### 19.3 Don't Skip Edge Cases

Test edge cases explicitly:

```cpp
TEST(FrameTest, HandlesZeroWidth) { ... }
TEST(FrameTest, HandlesZeroHeight) { ... }
TEST(FrameTest, HandlesMaximumSize) { ... }
TEST(FrameTest, HandlesEmptyData) { ... }
TEST(FrameTest, HandlesNullData) { ... }
```

### 19.4 Don't Ignore Test Failures

Never:

* Comment out failing tests
* Skip tests without documentation
* Mark tests as "expected failure" without fixing
* Merge code with known test failures

### 19.5 Don't Write Brittle Tests

Bad:

```cpp
TEST(JsonTest, ExactStringMatch) {
    // Brittle: breaks on any formatting change
    EXPECT_EQ(json_string, "{\"key\": \"value\"}");
}
```

Good:

```cpp
TEST(JsonTest, ContainsRequiredFields) {
    auto json = parse_json(json_string);
    EXPECT_TRUE(json.contains("key"));
    EXPECT_EQ(json["key"], "value");
}
```

---

## 20. Test Metrics and Reporting

### 20.1 Coverage Targets

Minimum coverage targets:

| Metric | Target |
|--------|--------|
| Line coverage | 80% |
| Branch coverage | 70% |
| Function coverage | 90% |
| Critical path coverage | 100% |

### 20.2 Test Reports

Generate test reports for CI/CD:

```bash
# Generate JUnit XML report
ctest --output-junit test-results.xml

# Generate coverage report
cmake --build . --target coverage
```

### 20.3 Test Dashboard

Track test health:

* Test pass rate
* Test execution time
* Coverage trends
* Flaky test identification

---

## 21. Clean, Readable Code — No Unnecessary Comments

Code must be self-documenting through clear naming and structure.

Bad:

```cpp
// Calculate the angle between two vectors
float angle = calculateAngle(v1, v2);

// Check if the result is valid
if (angle > 0.0f) {
    // Store the result
    results.push_back(angle);
}
```

Good:

```cpp
float angle_degrees = calculate_angle_between_vectors(upper_arm, reference);
if (angle_degrees > 0.0f) {
    rom_results.push_back(angle_degrees);
}
```

Rules:

* Do NOT write comments that merely restate what the code does.
* Do NOT add "TODO" comments for things that should just be done.
* Do NOT add "removed" or "changed" comments documenting history (use git).
* Do NOT add section dividers or decoration comments.
* Do NOT add author/date/version comments in code (use git).

A comment is justified ONLY when it explains WHY, not WHAT:

* why a non-obvious constraint exists
* why a workaround is needed for a specific bug
* why a particular algorithm was chosen over an obvious alternative
* why a seemingly wrong value is actually correct

If removing the comment would not confuse a future reader, do not write it.

---

## 22. Documentation

Document decisions that are not obvious.

Do not write documentation that merely repeats the code.

Good documentation explains:

* why an abstraction exists
* why a dependency exists
* why a hardware limitation exists
* why a particular model was selected
* why a performance tradeoff was made

---

## 23. Changes Must Be Incremental

Prefer small, reviewable changes.

Do not rewrite the entire architecture to solve a small bug.

When implementing a feature:

1. Understand existing code.
2. Identify the smallest change.
3. Write test for the change.
4. Implement it.
5. Verify tests pass.
6. Measure it.
7. Document only what is necessary.

---

## 24. Preserve Existing Architecture

Do not introduce a new architecture simply because another architecture is theoretically cleaner.

The existing project conventions are part of the product.

Change architecture only when there is a demonstrated problem.

---

## 25. No Fake Implementations

Do not create fake production functionality merely to make compilation pass.

If hardware is unavailable:

* create an explicit interface
* create a test/mock implementation where useful
* clearly mark hardware-dependent functionality

Never silently pretend that inference or camera capture succeeded.

---

## 26. No Hidden Assumptions

If implementation depends on:

* JetPack version
* Orbbec SDK version
* ONNX Runtime version
* CUDA version
* model input shape
* camera resolution
* hardware capability

make the dependency explicit.

Do not assume a version that has not been verified.

---

## 27. Security

Treat all external input as untrusted.

Validate:

* HTTP parameters
* JSON
* configuration
* file paths
* database input

Avoid command execution.

Avoid unnecessary privileges.

---

## 28. AI Agent Behavior

When working on Oculus, the coding agent must:

1. Read the relevant existing code before modifying it.
2. **Write tests before implementation (TDD).**
3. Prefer minimal changes.
4. Explain architectural changes before making large changes.
5. Never silently introduce a major dependency.
6. Never silently introduce CUDA coupling.
7. Never silently change the public API.
8. Never silently change database schema.
9. **Run all tests after modifications.**
10. **Verify test coverage meets requirements.**
11. Report failures honestly.
12. Never claim something works without verification.

---

## 29. When Unsure

Do not guess about hardware behavior.

Do not invent:

* SDK APIs
* camera capabilities
* ONNX Runtime APIs
* Jetson capabilities
* model input/output formats

If the information is unavailable, state the uncertainty and isolate the assumption.

---

## 30. Priority Order

When making engineering decisions, prioritize:

```text
Correctness
    ↓
Testability
    ↓
Maintainability
    ↓
Portability
    ↓
Observability
    ↓
Performance
    ↓
Convenience
```

Do not sacrifice architecture for a premature benchmark result.

Do not sacrifice correctness for speed of implementation.

**Do not sacrifice testability for convenience.**

---

## 31. Final Principle

Oculus should feel like a small embedded product, not a research project.

The desired result is:

```text
        ┌─────────────────────┐
        │       Oculus        │
        │                     │
        │ Camera              │
        │ Inference           │
        │ Pose                │
        │ Storage             │
        │ HTTP                │
        │ Metrics             │
        │                     │
        └─────────────────────┘
                  │
                  ▼
             ./oculus
```

One application.

One product version.

One runtime binary.

Hardware acceleration is an implementation detail.

**Every component has tests.**

**Every change has a test.**

**Every bug fix has a regression test.**

Keep the core small.
Keep the boundaries clean.
Measure before optimizing.
Build for today's MVP without closing tomorrow's options.

