include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 0.8.0
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.18.3
    GIT_SHALLOW TRUE
)

if(BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
        GIT_SHALLOW TRUE
    )
endif()

# ONNX Runtime — platform-specific pre-built binary
set(ORT_VERSION "1.20.1")
if(APPLE AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    set(ORT_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/onnxruntime-osx-arm64-${ORT_VERSION}.tgz")
elseif(UNIX AND CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(ORT_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/onnxruntime-linux-aarch64-${ORT_VERSION}.tgz")
elseif(UNIX AND CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(ORT_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/onnxruntime-linux-x86_64-${ORT_VERSION}.tgz")
else()
    message(WARNING "ONNX Runtime: unsupported platform ${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}")
    set(ORT_URL "")
endif()

if(NOT ORT_URL STREQUAL "")
    FetchContent_Declare(onnxruntime URL ${ORT_URL})
endif()

# OpenCV — minimal build (imgcodecs + imgproc)
set(_OCULUS_BUILD_TESTS ${BUILD_TESTS})
FetchContent_Declare(
    opencv
    GIT_REPOSITORY https://github.com/opencv/opencv.git
    GIT_TAG 4.10.0
    GIT_SHALLOW TRUE
)
set(BUILD_opencv_apps OFF CACHE BOOL "" FORCE)
set(BUILD_PERF_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_python2 OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_python3 OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_java OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_js OFF CACHE BOOL "" FORCE)
set(WITH_GTK OFF CACHE BOOL "" FORCE)
set(WITH_QT OFF CACHE BOOL "" FORCE)
set(WITH_VTK OFF CACHE BOOL "" FORCE)
set(WITH_CUDA OFF CACHE BOOL "" FORCE)
set(WITH_OPENCL OFF CACHE BOOL "" FORCE)
set(WITH_OPENEXR OFF CACHE BOOL "" FORCE)
set(WITH_FFMPEG OFF CACHE BOOL "" FORCE)
set(WITH_GSTREAMER OFF CACHE BOOL "" FORCE)
set(WITH_1394 OFF CACHE BOOL "" FORCE)
set(WITH_AVFOUNDATION OFF CACHE BOOL "" FORCE)
set(WITH_OBSENSOR OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_videoio OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_highgui OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_flann OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_photo OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_stitching OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_features2d OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_calib3d OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_objdetect OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_ml OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_dnn OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_gapi OFF CACHE BOOL "" FORCE)

message(STATUS "Fetching dependencies...")
FetchContent_MakeAvailable(nlohmann_json spdlog yaml-cpp httplib opencv)

# Restore BUILD_TESTS after OpenCV (OpenCV forces it OFF)
set(BUILD_TESTS ${_OCULUS_BUILD_TESTS} CACHE BOOL "" FORCE)

if(BUILD_TESTS)
    FetchContent_MakeAvailable(googletest)
endif()

if(NOT ORT_URL STREQUAL "")
    FetchContent_MakeAvailable(onnxruntime)

    # ONNX Runtime pre-built binary layout
    set(ORT_ROOT "${onnxruntime_SOURCE_DIR}")
    set(ORT_INCLUDE_DIR "${ORT_ROOT}/include")
    set(ORT_LIB_DIR "${ORT_ROOT}/lib")

    add_library(onnxruntime::onnxruntime SHARED IMPORTED)
    if(APPLE)
        set_target_properties(onnxruntime::onnxruntime PROPERTIES
            IMPORTED_LOCATION "${ORT_LIB_DIR}/libonnxruntime.dylib"
            INTERFACE_INCLUDE_DIRECTORIES "${ORT_INCLUDE_DIR}"
        )
    else()
        set_target_properties(onnxruntime::onnxruntime PROPERTIES
            IMPORTED_LOCATION "${ORT_LIB_DIR}/libonnxruntime.so"
            INTERFACE_INCLUDE_DIRECTORIES "${ORT_INCLUDE_DIR}"
        )
    endif()

    message(STATUS "ONNX Runtime: ${ORT_ROOT}")
endif()

message(STATUS "Dependencies fetched successfully")