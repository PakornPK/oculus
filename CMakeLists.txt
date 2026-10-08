cmake_minimum_required(VERSION 3.15)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
project(Oculus VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTS "Build tests" OFF)
option(BUILD_DEMO "Build demo application" OFF)

include(cmake/Dependencies.cmake)

add_library(oculus_lib STATIC
    src/core/frame.cpp
    src/core/pose.cpp
    src/core/hardware_detector.cpp
    src/camera/file_camera.cpp
    src/camera/camera_factory.cpp
)
target_include_directories(oculus_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(oculus_lib
    PUBLIC
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        yaml-cpp::yaml-cpp
)

if(NOT CMAKE_TRY_COMPILE_TARGET_TYPE STREQUAL "STATIC_LIBRARY")
    add_executable(oculus apps/oculus-sensor/main.cpp)
    target_link_libraries(oculus PRIVATE oculus_lib)
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()