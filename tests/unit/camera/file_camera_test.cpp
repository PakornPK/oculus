#include <gtest/gtest.h>
#include "oculus/camera/file_camera.hpp"
#include "oculus/camera/camera_factory.hpp"

using namespace oculus;

TEST(FileCameraTest, OpensAndCloses) {
    FileCamera camera("test.mp4");
    EXPECT_FALSE(camera.is_open());

    EXPECT_TRUE(camera.open());
    EXPECT_TRUE(camera.is_open());

    camera.close();
    EXPECT_FALSE(camera.is_open());
}

TEST(FileCameraTest, CaptureReturnsFrame) {
    FileCamera camera("test.mp4");
    camera.open();

    Frame frame = camera.capture();
    EXPECT_EQ(frame.width, 640);
    EXPECT_EQ(frame.height, 480);
    EXPECT_TRUE(frame.is_valid());
    EXPECT_EQ(frame.frame_id, 0);
}

TEST(FileCameraTest, CaptureIncrementsFrameId) {
    FileCamera camera("test.mp4");
    camera.open();

    Frame f1 = camera.capture();
    Frame f2 = camera.capture();
    EXPECT_EQ(f1.frame_id, 0);
    EXPECT_EQ(f2.frame_id, 1);
}

TEST(FileCameraTest, CaptureThrowsWhenNotOpen) {
    FileCamera camera("test.mp4");
    EXPECT_THROW(camera.capture(), std::runtime_error);
}

TEST(FileCameraTest, InfoReturnsPath) {
    FileCamera camera("/data/test.mp4");
    auto info = camera.info();
    EXPECT_EQ(info.name, "FileCamera");
    EXPECT_EQ(info.device_path, "/data/test.mp4");
    EXPECT_FALSE(info.has_depth);
}

TEST(CameraFactoryTest, CreateFileCamera) {
    auto camera = CameraFactory::create_file_camera("test.mp4");
    EXPECT_NE(camera, nullptr);
    EXPECT_FALSE(camera->is_open());
}

TEST(CameraFactoryTest, CreateWithUnknownDeviceReturnsNull) {
    CameraConfig config;
    config.device = "unknown";
    auto camera = CameraFactory::create(config);
    EXPECT_EQ(camera, nullptr);
}