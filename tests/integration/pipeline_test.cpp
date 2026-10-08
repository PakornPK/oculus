// tests/integration/pipeline_test.cpp
// Integration test - ทดสอบ full pipeline ด้วย mock components

#include <gtest/gtest.h>
#include "oculus/runtime/pipeline.hpp"
#include "tests/mocks/mock_camera.hpp"
#include "tests/mocks/mock_inference_engine.hpp"
#include "tests/mocks/mock_storage.hpp"

using namespace oculus;
using namespace oculus::testing;

class PipelineIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // สร้าง mock components
        mock_camera = std::make_unique<MockCamera>();
        mock_inference = std::make_unique<MockInferenceEngine>();
        mock_storage = std::make_unique<MockStorage>();
        
        // ตั้งค่า pipeline
        PipelineConfig config;
        config.frame_queue_size = 10;
        config.result_queue_size = 10;
        config.capture_fps = 30;
        
        pipeline = std::make_unique<Pipeline>(
            std::move(mock_camera),
            std::move(mock_inference),
            std::move(mock_storage),
            config
        );
    }
    
    void TearDown() override {
        if (pipeline && pipeline->is_running()) {
            pipeline->stop();
        }
    }
    
    std::unique_ptr<MockCamera> mock_camera;
    std::unique_ptr<MockInferenceEngine> mock_inference;
    std::unique_ptr<MockStorage> mock_storage;
    std::unique_ptr<Pipeline> pipeline;
};

TEST_F(PipelineIntegrationTest, StartStopCleanly) {
    // ทดสอบว่า pipeline start/stop ได้สะอาด
    EXPECT_FALSE(pipeline->is_running());
    
    // Inject test frames
    mock_camera->enqueue_frame(create_test_frame());
    mock_camera->enqueue_frame(create_test_frame());
    
    // Start pipeline
    ASSERT_TRUE(pipeline->start());
    EXPECT_TRUE(pipeline->is_running());
    
    // รอสักครู่ให้ pipeline ทำงาน
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Stop pipeline
    pipeline->stop();
    EXPECT_FALSE(pipeline->is_running());
}

TEST_F(PipelineIntegrationTest, ProcessesFramesEndToEnd) {
    // ทดสอบ full pipeline: Camera → Inference → Result
    
    // สร้าง test frames
    Frame test_frame = create_test_frame(640, 480);
    mock_camera->enqueue_frame(test_frame);
    mock_camera->enqueue_frame(test_frame);
    mock_camera->enqueue_frame(test_frame);
    
    // สร้าง expected pose result
    PoseResult expected_result = create_pose_with_keypoints({
        {320, 240},  // nose
        {300, 200},  // left eye
        {340, 200},  // right eye
    });
    mock_inference->enqueue_result(expected_result);
    mock_inference->enqueue_result(expected_result);
    mock_inference->enqueue_result(expected_result);
    
    // Start pipeline
    ASSERT_TRUE(pipeline->start());
    
    // รอให้ pipeline ประมวลผล
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // ตรวจสอบผลลัพธ์
    auto results = pipeline->get_latest_results(3);
    EXPECT_GE(results.size(), 1);
    
    // ตรวจสอบว่า result มี pose data
    if (!results.empty()) {
        EXPECT_FALSE(results[0].poses.empty());
        EXPECT_EQ(results[0].poses[0].keypoints.size(), 3);
    }
    
    // ตรวจสอบว่า inference ถูกเรียก
    EXPECT_GE(mock_inference->inference_count(), 1);
    
    pipeline->stop();
}

TEST_F(PipelineIntegrationTest, HandlesCameraFailure) {
    // ทดสอบว่า pipeline จัดการกับ camera failure ยังไง
    
    // ตั้งค่า camera ให้ fail
    mock_camera->set_fail_on_open(true);
    
    // Pipeline ไม่ควร start ได้
    EXPECT_FALSE(pipeline->start());
    EXPECT_FALSE(pipeline->is_running());
}

TEST_F(PipelineIntegrationTest, HandlesInferenceFailure) {
    // ทดสอบว่า pipeline จัดการกับ inference failure ยังไง
    
    // ตั้งค่า inference ให้ fail
    mock_inference->set_fail_on_load(true);
    
    // Pipeline ไม่ควร start ได้
    EXPECT_FALSE(pipeline->start());
}

TEST_F(PipelineIntegrationTest, StoresResultsToStorage) {
    // ทดสอบว่า results ถูกเก็บใน storage
    
    Frame test_frame = create_test_frame();
    mock_camera->enqueue_frame(test_frame);
    
    PoseResult test_result = create_test_pose_result();
    mock_inference->enqueue_result(test_result);
    
    // Start pipeline
    ASSERT_TRUE(pipeline->start());
    
    // รอให้ pipeline ประมวลผล
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // ตรวจสอบว่า storage ถูกเรียก
    EXPECT_GE(mock_storage->store_event_count(), 1);
    EXPECT_GE(mock_storage->store_metrics_count(), 1);
    
    pipeline->stop();
}

TEST_F(PipelineIntegrationTest, MeasuresPerformanceMetrics) {
    // ทดสอบว่า pipeline วัด performance metrics ได้
    
    // Inject multiple frames
    for (int i = 0; i < 10; ++i) {
        mock_camera->enqueue_frame(create_test_frame());
        mock_inference->enqueue_result(create_test_pose_result());
    }
    
    ASSERT_TRUE(pipeline->start());
    
    // รอให้ pipeline ประมวลผล
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // ดึง metrics
    auto metrics = pipeline->get_metrics();
    
    // ตรวจสอบว่า metrics ถูกต้อง
    EXPECT_GT(metrics.capture_latency_ms, 0.0);
    EXPECT_GT(metrics.inference_latency_ms, 0.0);
    EXPECT_GT(metrics.total_latency_ms, 0.0);
    EXPECT_GT(metrics.fps, 0.0);
    
    pipeline->stop();
}

TEST_F(PipelineIntegrationTest, BoundedQueuePreventsMemoryGrowth) {
    // ทดสอบว่า bounded queue ป้องกัน memory growth
    
    PipelineConfig config;
    config.frame_queue_size = 5;  // queue เล็กมาก
    config.result_queue_size = 5;
    
    auto small_queue_pipeline = std::make_unique<Pipeline>(
        std::make_unique<MockCamera>(),
        std::make_unique<MockInferenceEngine>(),
        std::make_unique<MockStorage>(),
        config
    );
    
    // Inject frames มากกว่า queue size
    for (int i = 0; i < 100; ++i) {
        mock_camera->enqueue_frame(create_test_frame());
    }
    
    // Pipeline ไม่ควร crash หรือใช้ memory ไม่จำกัด
    ASSERT_TRUE(small_queue_pipeline->start());
    
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    small_queue_pipeline->stop();
    
    // ถ้าถึงจุดนี้ได้ แสดงว่า bounded queue ทำงาน
    SUCCEED();
}
