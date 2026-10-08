enable_testing()

include(GoogleTest)

function(oculus_add_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME}
        PRIVATE
            oculus_lib
            GTest::gtest
            GTest::gtest_main
    )
    target_include_directories(${TEST_NAME}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/tests/mocks
    )
    gtest_discover_tests(${TEST_NAME})
endfunction()

oculus_add_test(frame_test unit/core/frame_test.cpp)
oculus_add_test(pose_test unit/core/pose_test.cpp)
oculus_add_test(hardware_detector_test unit/core/hardware_detector_test.cpp)
oculus_add_test(bounded_queue_test unit/core/bounded_queue_test.cpp)
oculus_add_test(file_camera_test unit/camera/file_camera_test.cpp)

oculus_add_test(joint_angle_calculator_test unit/rom/joint_angle_calculator_test.cpp)
oculus_add_test(shoulder_rom_analyzer_test unit/rom/shoulder_rom_analyzer_test.cpp)

if(TARGET onnxruntime::onnxruntime)
    oculus_add_test(onnx_inference_engine_test unit/inference/onnx_inference_engine_test.cpp)
endif()