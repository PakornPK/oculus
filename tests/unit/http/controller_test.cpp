// tests/unit/http/controller_test.cpp
// ทดสอบ HTTP controllers

#include <gtest/gtest.h>
#include "oculus/http/controllers/health_controller.hpp"
#include "oculus/http/controllers/metrics_controller.hpp"
#include "oculus/http/controllers/pose_controller.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class HealthControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        controller = std::make_unique<HealthController>();
    }
    
    std::unique_ptr<HealthController> controller;
};

TEST_F(HealthControllerTest, HealthEndpointReturnsOk) {
    auto response = controller->handle_health();
    
    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.content_type, "application/json");
    
    auto body = json::parse(response.body);
    EXPECT_EQ(body["status"], "ok");
}

TEST_F(HealthControllerTest, SystemInfoContainsRequiredFields) {
    auto response = controller->handle_system_info();
    
    EXPECT_EQ(response.status_code, 200);
    
    auto body = json::parse(response.body);
    
    // ตรวจสอบ required fields
    EXPECT_TRUE(body.contains("product"));
    EXPECT_TRUE(body.contains("version"));
    EXPECT_TRUE(body.contains("platform"));
    EXPECT_TRUE(body.contains("model"));
    EXPECT_TRUE(body.contains("inference_backend"));
    
    EXPECT_EQ(body["product"], "oculus");
    EXPECT_FALSE(body["version"].get<std::string>().empty());
}

class MetricsControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        controller = std::make_unique<MetricsController>();
        
        // Inject test metrics
        PipelineMetrics metrics;
        metrics.capture_latency_ms = 4.5;
        metrics.preprocess_latency_ms = 3.2;
        metrics.inference_latency_ms = 18.1;
        metrics.postprocess_latency_ms = 2.0;
        metrics.total_latency_ms = 27.8;
        metrics.fps = 36.0;
        metrics.cpu_usage_percent = 45.0;
        metrics.memory_usage_mb = 256.0;
        
        controller->update_metrics(metrics);
    }
    
    std::unique_ptr<MetricsController> controller;
};

TEST_F(MetricsControllerTest, MetricsEndpointReturnsAllFields) {
    auto response = controller->handle_metrics();
    
    EXPECT_EQ(response.status_code, 200);
    
    auto body = json::parse(response.body);
    
    // ตรวจสอบ latency fields
    EXPECT_TRUE(body.contains("capture_latency_ms"));
    EXPECT_TRUE(body.contains("preprocess_latency_ms"));
    EXPECT_TRUE(body.contains("inference_latency_ms"));
    EXPECT_TRUE(body.contains("postprocess_latency_ms"));
    EXPECT_TRUE(body.contains("total_latency_ms"));
    
    // ตรวจสอบ performance fields
    EXPECT_TRUE(body.contains("fps"));
    EXPECT_TRUE(body.contains("cpu_usage_percent"));
    EXPECT_TRUE(body.contains("memory_usage_mb"));
    
    // ตรวจสอบค่า
    EXPECT_DOUBLE_EQ(body["capture_latency_ms"], 4.5);
    EXPECT_DOUBLE_EQ(body["inference_latency_ms"], 18.1);
    EXPECT_DOUBLE_EQ(body["total_latency_ms"], 27.8);
    EXPECT_DOUBLE_EQ(body["fps"], 36.0);
}

TEST_F(MetricsControllerTest, StatusEndpointReturnsPipelineStatus) {
    auto response = controller->handle_status();
    
    EXPECT_EQ(response.status_code, 200);
    
    auto body = json::parse(response.body);
    
    EXPECT_TRUE(body.contains("camera_status"));
    EXPECT_TRUE(body.contains("inference_status"));
    EXPECT_TRUE(body.contains("pipeline_status"));
    EXPECT_TRUE(body.contains("uptime_seconds"));
}

class PoseControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        controller = std::make_unique<PoseController>();
    }
    
    std::unique_ptr<PoseController> controller;
};

TEST_F(PoseControllerTest, PoseEndpointReturnsLatestResult) {
    // Inject test pose result
    PoseResult result;
    result.timestamp = 1234567890;
    
    Pose pose;
    pose.confidence = 0.95f;
    
    Keypoint nose;
    nose.x = 320.0f;
    nose.y = 240.0f;
    nose.confidence = 0.98f;
    pose.keypoints.push_back(nose);
    
    Keypoint left_eye;
    left_eye.x = 300.0f;
    left_eye.y = 200.0f;
    left_eye.confidence = 0.95f;
    pose.keypoints.push_back(left_eye);
    
    result.poses.push_back(pose);
    
    controller->update_pose(result);
    
    // Test endpoint
    auto response = controller->handle_get_pose();
    
    EXPECT_EQ(response.status_code, 200);
    
    auto body = json::parse(response.body);
    
    EXPECT_TRUE(body.contains("timestamp"));
    EXPECT_TRUE(body.contains("poses"));
    EXPECT_EQ(body["timestamp"], 1234567890);
    
    auto poses = body["poses"];
    EXPECT_EQ(poses.size(), 1);
    
    auto keypoints = poses[0]["keypoints"];
    EXPECT_EQ(keypoints.size(), 2);
    
    EXPECT_DOUBLE_EQ(keypoints[0]["x"], 320.0);
    EXPECT_DOUBLE_EQ(keypoints[0]["y"], 240.0);
    EXPECT_DOUBLE_EQ(keypoints[0]["confidence"], 0.98);
}

TEST_F(PoseControllerTest, PoseEndpointReturnsEmptyWhenNoData) {
    auto response = controller->handle_get_pose();
    
    EXPECT_EQ(response.status_code, 200);
    
    auto body = json::parse(response.body);
    EXPECT_TRUE(body.contains("poses"));
    EXPECT_TRUE(body["poses"].empty());
}

TEST_F(PoseControllerTest, PoseEndpointReturnsMultiplePeople) {
    // Inject result with multiple people
    PoseResult result;
    result.timestamp = 1234567890;
    
    // Person 1
    Pose pose1;
    pose1.confidence = 0.95f;
    Keypoint kp1;
    kp1.x = 100.0f;
    kp1.y = 200.0f;
    kp1.confidence = 0.9f;
    pose1.keypoints.push_back(kp1);
    result.poses.push_back(pose1);
    
    // Person 2
    Pose pose2;
    pose2.confidence = 0.90f;
    Keypoint kp2;
    kp2.x = 400.0f;
    kp2.y = 250.0f;
    kp2.confidence = 0.85f;
    pose2.keypoints.push_back(kp2);
    result.poses.push_back(pose2);
    
    controller->update_pose(result);
    
    auto response = controller->handle_get_pose();
    auto body = json::parse(response.body);
    
    EXPECT_EQ(body["poses"].size(), 2);
}
