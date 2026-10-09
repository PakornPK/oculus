cmake_minimum_required(VERSION 3.15)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
project(Oculus VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTS "Build tests" OFF)
option(BUILD_DEMO "Build demo application" OFF)

include(cmake/Dependencies.cmake)

set(OCULUS_SOURCES
    src/core/frame.cpp
    src/core/pose.cpp
    src/core/hardware_detector.cpp
    src/camera/file_camera.cpp
    src/camera/camera_factory.cpp
    src/rom/joint_angle_calculator.cpp
    src/rom/shoulder_rom_analyzer.cpp
    src/rom/session_manager.cpp
    src/rom/calibrator.cpp
    src/rom/recipe.cpp
    src/http/web_server.cpp
    src/storage/storage.cpp
    src/runtime/watchdog.cpp
    src/runtime/metrics.cpp
    src/core/config.cpp
)

if(TARGET onnxruntime::onnxruntime)
    list(APPEND OCULUS_SOURCES
        src/inference/onnx_inference_engine.cpp
        src/inference/person_detector.cpp
        src/inference/models/rtmpose/rtmpose_preprocessor.cpp
        src/inference/models/rtmpose/rtmpose_postprocessor.cpp
    )
endif()

add_library(oculus_lib STATIC ${OCULUS_SOURCES})
target_include_directories(oculus_lib
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${opencv_SOURCE_DIR}/modules/core/include
        ${opencv_SOURCE_DIR}/modules/imgproc/include
        ${opencv_SOURCE_DIR}/modules/imgcodecs/include
        ${opencv_SOURCE_DIR}/modules/videoio/include
        ${opencv_BINARY_DIR}
        ${CMAKE_BINARY_DIR}
)
target_link_libraries(oculus_lib
    PUBLIC
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        yaml-cpp::yaml-cpp
        httplib
        opencv_core
        opencv_imgproc
        opencv_imgcodecs
        opencv_videoio
)

if(TARGET onnxruntime::onnxruntime)
    target_link_libraries(oculus_lib PUBLIC onnxruntime::onnxruntime)
    target_compile_definitions(oculus_lib PUBLIC OCULUS_HAS_ONNXRUNTIME=1)
endif()

if(NOT CMAKE_TRY_COMPILE_TARGET_TYPE STREQUAL "STATIC_LIBRARY")
    add_executable(oculus apps/oculus-sensor/main.cpp)
    target_link_libraries(oculus PRIVATE oculus_lib)
    target_include_directories(oculus PRIVATE
        ${opencv_SOURCE_DIR}/modules/core/include
        ${opencv_SOURCE_DIR}/modules/imgproc/include
        ${opencv_SOURCE_DIR}/modules/imgcodecs/include
        ${opencv_SOURCE_DIR}/modules/videoio/include
        ${opencv_BINARY_DIR}
        ${CMAKE_BINARY_DIR}
    )
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()