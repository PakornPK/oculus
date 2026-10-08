// tests/unit/inference/preprocessor_test.cpp
// ทดสอบ preprocessing logic โดยไม่ต้องโหลด model

#include <gtest/gtest.h>
#include "oculus/inference/models/rtmpose/rtmpose_preprocessor.hpp"

TEST(RTMPosePreprocessorTest, ResizeToModelInputSize) {
    // สร้าง frame ขนาด 640x480
    Frame frame;
    frame.width = 640;
    frame.height = 480;
    frame.data = std::vector<uint8_t>(640 * 480 * 3, 128);  // RGB
    frame.format = PixelFormat::RGB;
    
    RTMPosePreprocessor preprocessor(256, 192);  // model input size
    
    auto input_tensor = preprocessor.process(frame);
    
    // ตรวจสอบ output size
    EXPECT_EQ(input_tensor.shape[0], 1);      // batch
    EXPECT_EQ(input_tensor.shape[1], 3);      // channels (RGB)
    EXPECT_EQ(input_tensor.shape[2], 192);    // height
    EXPECT_EQ(input_tensor.shape[3], 256);    // width
}

TEST(RTMPosePreprocessorTest, NormalizesPixelValues) {
    // ทดสอบว่า normalize ค่า pixel เป็น [0, 1] หรือ [-1, 1]
    Frame frame;
    frame.width = 2;
    frame.height = 2;
    frame.data = {255, 128, 0, 255, 128, 0, 255, 128, 0, 255, 128, 0};  // 4 pixels RGB
    frame.format = PixelFormat::RGB;
    
    RTMPosePreprocessor preprocessor(2, 2);
    auto input_tensor = preprocessor.process(frame);
    
    // ตรวจสอบ normalization
    // ถ้า normalize เป็น [0, 1]: 255 → 1.0, 128 → ~0.5, 0 → 0.0
    // ถ้า normalize เป็น [-1, 1]: 255 → 1.0, 128 → ~0.0, 0 → -1.0
    
    // ตรวจสอบค่าที่ normalize แล้ว
    float max_val = *std::max_element(input_tensor.data.begin(), input_tensor.data.end());
    float min_val = *std::min_element(input_tensor.data.begin(), input_tensor.data.end());
    
    EXPECT_LE(max_val, 1.0f);
    EXPECT_GE(min_val, -1.0f);
}

TEST(RTMPosePreprocessorTest, HandlesGrayscaleInput) {
    // ทดสอบ grayscale input
    Frame frame;
    frame.width = 4;
    frame.height = 4;
    frame.data = std::vector<uint8_t>(16, 200);
    frame.format = PixelFormat::GRAY;
    
    RTMPosePreprocessor preprocessor(4, 4);
    auto input_tensor = preprocessor.process(frame);
    
    // ควรมี 3 channels (convert grayscale → RGB)
    EXPECT_EQ(input_tensor.shape[1], 3);
}

TEST(RTMPosePreprocessorTest, HandlesInvalidFrame) {
    // ทดสอบ frame ที่ไม่ valid
    Frame frame;
    frame.width = 0;
    frame.height = 0;
    frame.data = {};
    frame.format = PixelFormat::RGB;
    
    RTMPosePreprocessor preprocessor(256, 192);
    
    // ควร throw exception หรือ return error
    EXPECT_THROW(preprocessor.process(frame), std::runtime_error);
}
