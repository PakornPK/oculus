# tests/CMakeLists.txt
# Test configuration for Oculus project

enable_testing()

# ค้นหา Google Test
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# Helper function สำหรับสร้าง test executable
function(oculus_add_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME}
        PRIVATE
            oculus_lib
            GTest::gtest
            GTest::gtest_main
            ${ARGN}
    )
    target_include_directories(${TEST_NAME}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/tests/mocks
    )
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

# Unit Tests
oculus_add_test(frame_test unit/core/frame_test.cpp)
oculus_add_test(pose_test unit/core/pose_test.cpp)
oculus_add_test(config_test unit/core/config_test.cpp)
oculus_add_test(preprocessor_test unit/inference/preprocessor_test.cpp)
oculus_add_test(postprocessor_test unit/inference/postprocessor_test.cpp)
oculus_add_test(sqlite_storage_test unit/storage/sqlite_storage_test.cpp)
oculus_add_test(controller_test unit/http/controller_test.cpp)

# Integration Tests (may require hardware)
oculus_add_test(pipeline_test integration/pipeline_test.cpp)

# สร้าง test library สำหรับ shared test utilities
add_library(test_utils STATIC
    mocks/mock_camera.hpp
    mocks/mock_inference_engine.hpp
    mocks/mock_storage.hpp
)
target_include_directories(test_utils
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/tests/mocks
)

# Test configuration
set(CMAKE_CTEST_ARGUMENTS "--output-on-failure")
