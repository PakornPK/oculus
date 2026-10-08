// tests/unit/inference/postprocessor_test.cpp
// ทดสอบ postprocessing logic - แปลง model output เป็น keypoints

#include <gtest/gtest.h>
#include "oculus/inference/models/rtmpose/rtmpose_postprocessor.hpp"

class RTMPosePostprocessorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // สร้าง mock heatmap output จาก RTMPose
        // RTMPose output: [1, 17, 64, 48] (batch, keypoints, H, W)
        mock_heatmap = create_mock_heatmap();
    }
    
    InferenceResult create_mock_heatmap() {
        InferenceResult result;
        result.output_shape = {1, 17, 64, 48};  // 17 keypoints
        
        // สร้าง heatmap ที่มี peak ที่ตำแหน่งที่รู้จัก
        // peak ที่ (24, 32) สำหรับ keypoint แรก
        std::vector<float> data(1 * 17 * 64 * 48, 0.0f);
        
        // สร้าง peak ที่ keypoint 0, location (24, 32)
        int keypoint_0_offset = 0 * 64 * 48;
        data[keypoint_0_offset + 32 * 48 + 24] = 0.95f;  // peak
        data[keypoint_0_offset + 31 * 48 + 24] = 0.80f;  // neighbors
        data[keypoint_0_offset + 33 * 48 + 24] = 0.80f;
        data[keypoint_0_offset + 32 * 48 + 23] = 0.80f;
        data[keypoint_0_offset + 32 * 48 + 25] = 0.80f;
        
        result.output_data = data;
        return result;
    }
    
    InferenceResult mock_heatmap;
};

TEST_F(RTMPosePostprocessorTest, ExtractsKeypointsFromHeatmap) {
    RTMPosePostprocessor postprocessor(
        /* input_width */ 256,
        /* input_height */ 192,
        /* output_width */ 48,
        /* output_height */ 64,
        /* num_keypoints */ 17
    );
    
    auto poses = postprocessor.process(mock_heatmap);
    
    // ควรมี 1 pose
    ASSERT_EQ(poses.size(), 1);
    
    // ควรมี 17 keypoints
    EXPECT_EQ(poses[0].keypoints.size(), 17);
    
    // keypoint 0 ควรมี confidence สูง
    EXPECT_GT(poses[0].keypoints[0].confidence, 0.9f);
    
    // keypoint 0 ควรอยู่ที่ตำแหน่งที่ถูกต้อง (scaled จาก 48x64 → 256x192)
    float expected_x = 24.0f * (256.0f / 48.0f);  // ~128
    float expected_y = 32.0f * (192.0f / 64.0f);  // ~96
    EXPECT_NEAR(poses[0].keypoints[0].x, expected_x, 5.0f);
    EXPECT_NEAR(poses[0].keypoints[0].y, expected_y, 5.0f);
}

TEST_F(RTMPosePostprocessorTest, FiltersLowConfidenceKeypoints) {
    // สร้าง heatmap ที่มี noise (confidence ต่ำ)
    InferenceResult noisy_heatmap;
    noisy_heatmap.output_shape = {1, 17, 64, 48};
    noisy_heatmap.output_data = std::vector<float>(1 * 17 * 64 * 48, 0.1f);  // low confidence
    
    RTMPosePostprocessor postprocessor(256, 192, 48, 64, 17);
    postprocessor.set_confidence_threshold(0.5f);
    
    auto poses = postprocessor.process(noisy_heatmap);
    
    // keypoints ที่มี confidence ต่ำควรถูก filter ออก
    for (const auto& pose : poses) {
        for (const auto& kp : pose.keypoints) {
            EXPECT_GE(kp.confidence, 0.5f);
        }
    }
}

TEST_F(RTMPosePostprocessorTest, HandlesMultiplePeople) {
    // สร้าง heatmap ที่มี 2 people
    InferenceResult multi_person_heatmap;
    multi_person_heatmap.output_shape = {1, 17, 64, 48};
    multi_person_heatmap.output_data = std::vector<float>(1 * 17 * 64 * 48, 0.0f);
    
    // Person 1: peak ที่ (10, 20)
    multi_person_heatmap.output_data[0 * 64 * 48 + 20 * 48 + 10] = 0.95f;
    
    // Person 2: peak ที่ (35, 40)
    multi_person_heatmap.output_data[0 * 64 * 48 + 40 * 48 + 35] = 0.90f;
    
    RTMPosePostprocessor postprocessor(256, 192, 48, 64, 17);
    auto poses = postprocessor.process(multi_person_heatmap);
    
    // ควรมี 2 poses
    EXPECT_GE(poses.size(), 1);  // อย่างน้อย 1 pose
}

TEST_F(RTMPosePostprocessorTest, CoordinateTransformCorrectness) {
    // ทดสอบว่า coordinate transform ถูกต้อง
    // heatmap 48x64 → original image 1920x1080
    
    RTMPosePostprocessor postprocessor(1920, 1080, 48, 64, 17);
    
    // สร้าง heatmap ที่ peak ที่ (0, 0) - top-left
    InferenceResult corner_heatmap;
    corner_heatmap.output_shape = {1, 17, 64, 48};
    corner_heatmap.output_data = std::vector<float>(1 * 17 * 64 * 48, 0.0f);
    corner_heatmap.output_data[0 * 64 * 48 + 0 * 48 + 0] = 0.99f;  // top-left corner
    
    auto poses = postprocessor.process(corner_heatmap);
    
    ASSERT_GE(poses.size(), 1);
    ASSERT_GE(poses[0].keypoints.size(), 1);
    
    // keypoint ควรอยู่ที่ top-left ของ original image
    EXPECT_NEAR(poses[0].keypoints[0].x, 0.0f, 10.0f);
    EXPECT_NEAR(poses[0].keypoints[0].y, 0.0f, 10.0f);
}
