#!/bin/bash
# scripts/run_tests.sh
# สคริปต์สำหรับรัน tests

set -e

echo "=== Oculus Test Suite ==="
echo ""

# สร้าง build directory ถ้ายังไม่มี
BUILD_DIR="build"
if [ ! -d "$BUILD_DIR" ]; then
    mkdir -p "$BUILD_DIR"
fi

cd "$BUILD_DIR"

# Configure with CMake
echo "Configuring CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON

# Build tests
echo "Building tests..."
cmake --build . --target all

echo ""
echo "=== Running Unit Tests ==="
echo ""

# รัน unit tests
ctest -R "unit/" --output-on-failure --verbose

echo ""
echo "=== Running Integration Tests ==="
echo ""

# รัน integration tests
ctest -R "integration/" --output-on-failure --verbose

echo ""
echo "=== Test Summary ==="
echo ""

# แสดง summary
ctest --output-on-failure

echo ""
echo "=== Tests Complete ==="
