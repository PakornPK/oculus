#include <gtest/gtest.h>
#include "oculus/camera/file_camera.hpp"
#include "oculus/camera/camera_factory.hpp"
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>
#include <filesystem>

using namespace oculus;

namespace fs = std::filesystem;

class FileCameraTest : public ::testing::Test {
protected:
    static std::string test_video_path_;

    static void SetUpTestSuite() {
        test_video_path_ = (fs::temp_directory_path() / "oculus_test_video.avi").string();

        const int width = 320;
        const int height = 240;
        const int fps = 10;
        const int num_frames = 30;

        cv::VideoWriter writer(test_video_path_,
                               cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
                               fps, cv::Size(width, height));
        if (!writer.isOpened()) {
            GTEST_SKIP() << "Cannot create test video (VideoWriter unavailable)";
            return;
        }

        for (int i = 0; i < num_frames; ++i) {
            cv::Mat frame(height, width, CV_8UC3, cv::Scalar(i * 8, 100, 200));
            writer.write(frame);
        }
        writer.release();
    }

    static void TearDownTestSuite() {
        if (!test_video_path_.empty() && fs::exists(test_video_path_)) {
            fs::remove(test_video_path_);
        }
    }
};

std::string FileCameraTest::test_video_path_;

TEST_F(FileCameraTest, OpensAndCloses) {
    if (test_video_path_.empty()) return;

    FileCamera camera(test_video_path_);
    EXPECT_FALSE(camera.is_open());

    EXPECT_TRUE(camera.open());
    EXPECT_TRUE(camera.is_open());

    camera.close();
    EXPECT_FALSE(camera.is_open());
}

TEST_F(FileCameraTest, OpenNonExistentFileFails) {
    FileCamera camera("/tmp/oculus_nonexistent_video.mp4");
    EXPECT_FALSE(camera.open());
    EXPECT_FALSE(camera.is_open());
}

TEST_F(FileCameraTest, OpenEmptyPathFails) {
    FileCamera camera("");
    EXPECT_FALSE(camera.open());
}

TEST_F(FileCameraTest, CaptureReturnsValidFrame) {
    if (test_video_path_.empty()) return;

    FileCamera camera(test_video_path_);
    ASSERT_TRUE(camera.open());

    Frame frame = camera.capture();
    EXPECT_EQ(frame.width, 320);
    EXPECT_EQ(frame.height, 240);
    EXPECT_EQ(frame.format, PixelFormat::RGB);
    EXPECT_TRUE(frame.is_valid());
    EXPECT_EQ(frame.frame_id, 0);
    EXPECT_GE(frame.timestamp, 0);
}

TEST_F(FileCameraTest, CaptureIncrementsFrameId) {
    if (test_video_path_.empty()) return;

    FileCamera camera(test_video_path_);
    ASSERT_TRUE(camera.open());

    Frame f1 = camera.capture();
    Frame f2 = camera.capture();
    EXPECT_EQ(f1.frame_id, 0);
    EXPECT_EQ(f2.frame_id, 1);
}

TEST_F(FileCameraTest, CaptureThrowsWhenNotOpen) {
    FileCamera camera(test_video_path_);
    EXPECT_THROW(camera.capture(), std::runtime_error);
}

TEST_F(FileCameraTest, InfoReturnsCorrectValues) {
    if (test_video_path_.empty()) return;

    FileCamera camera(test_video_path_);
    ASSERT_TRUE(camera.open());

    auto cam_info = camera.info();
    EXPECT_EQ(cam_info.name, "FileCamera");
    EXPECT_EQ(cam_info.device_path, test_video_path_);
    EXPECT_EQ(cam_info.max_width, 320);
    EXPECT_EQ(cam_info.max_height, 240);
    EXPECT_FALSE(cam_info.has_depth);
}

TEST_F(FileCameraTest, TotalFramesReported) {
    if (test_video_path_.empty()) return;

    FileCamera camera(test_video_path_);
    ASSERT_TRUE(camera.open());

    EXPECT_GT(camera.total_frames(), 0);
}

TEST_F(FileCameraTest, LoopResetsAfterEnd) {
    if (test_video_path_.empty()) return;

    FileCamera camera(test_video_path_);
    camera.set_loop(true);
    ASSERT_TRUE(camera.open());

    const int total = camera.total_frames();
    ASSERT_GT(total, 0);

    for (int i = 0; i < total; ++i) {
        camera.capture();
    }

    Frame next = camera.capture();
    EXPECT_EQ(next.frame_id, 0);
}

TEST_F(FileCameraTest, NoLoopThrowsAtEnd) {
    if (test_video_path_.empty()) return;

    FileCamera camera(test_video_path_);
    ASSERT_TRUE(camera.open());

    const int total = camera.total_frames();
    ASSERT_GT(total, 0);

    for (int i = 0; i < total; ++i) {
        camera.capture();
    }

    EXPECT_THROW(camera.capture(), std::runtime_error);
}

TEST_F(FileCameraTest, SeekToValidFrame) {
    if (test_video_path_.empty()) return;

    FileCamera camera(test_video_path_);
    ASSERT_TRUE(camera.open());

    ASSERT_TRUE(camera.seek(5));
    EXPECT_EQ(camera.current_frame(), 5);

    Frame frame = camera.capture();
    EXPECT_EQ(frame.frame_id, 5);
}

TEST_F(FileCameraTest, SeekOutOfBoundsFails) {
    if (test_video_path_.empty()) return;

    FileCamera camera(test_video_path_);
    ASSERT_TRUE(camera.open());

    EXPECT_FALSE(camera.seek(-1));
    EXPECT_FALSE(camera.seek(camera.total_frames() + 100));
}

TEST_F(FileCameraTest, SeekWhenClosedFails) {
    FileCamera camera(test_video_path_);
    EXPECT_FALSE(camera.seek(0));
}

TEST_F(FileCameraTest, FrameDataIsRgb) {
    if (test_video_path_.empty()) return;

    FileCamera camera(test_video_path_);
    ASSERT_TRUE(camera.open());

    Frame frame = camera.capture();
    ASSERT_TRUE(frame.is_valid());
    EXPECT_EQ(frame.data.size(),
              static_cast<size_t>(frame.width) * frame.height * 3);
}

TEST_F(FileCameraTest, MultipleOpenCloseCycles) {
    if (test_video_path_.empty()) return;

    FileCamera camera(test_video_path_);

    for (int cycle = 0; cycle < 3; ++cycle) {
        ASSERT_TRUE(camera.open());
        Frame frame = camera.capture();
        EXPECT_TRUE(frame.is_valid());
        camera.close();
        EXPECT_FALSE(camera.is_open());
    }
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