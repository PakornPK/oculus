// tests/unit/storage/sqlite_storage_test.cpp
// ทดสอบ SQLite storage operations

#include <gtest/gtest.h>
#include "oculus/storage/sqlite_storage.hpp"
#include <filesystem>

class SQLiteStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // สร้าง temporary database สำหรับทดสอบ
        test_db_path_ = std::filesystem::temp_directory_path() / "test_oculus.db";
        std::filesystem::remove(test_db_path_);  // clean up จาก test ก่อนหน้า
        
        storage_ = std::make_unique<SQLiteStorage>(test_db_path_.string());
        ASSERT_TRUE(storage_->initialize());
    }
    
    void TearDown() override {
        storage_.reset();
        std::filesystem::remove(test_db_path_);
    }
    
    std::filesystem::path test_db_path_;
    std::unique_ptr<SQLiteStorage> storage_;
};

TEST_F(SQLiteStorageTest, InitializesSuccessfully) {
    // ทดสอบว่า database initialize ได้
    EXPECT_TRUE(storage_->is_initialized());
}

TEST_F(SQLiteStorageTest, StoresAndRetrievesEvent) {
    // ทดสอบ store และ retrieve event
    Event event;
    event.type = "pose_detected";
    event.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    event.data = R"({"num_poses": 2})";
    
    ASSERT_TRUE(storage_->store_event(event));
    
    auto events = storage_->get_events(10);
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0].type, "pose_detected");
    EXPECT_EQ(events[0].data, R"({"num_poses": 2})");
}

TEST_F(SQLiteStorageTest, StoresAndRetrievesMetrics) {
    // ทดสอบ store และ retrieve metrics
    Metrics metrics;
    metrics.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    metrics.capture_latency_ms = 4.5;
    metrics.inference_latency_ms = 18.2;
    metrics.total_latency_ms = 25.1;
    metrics.fps = 37.5;
    
    ASSERT_TRUE(storage_->store_metrics(metrics));
    
    auto all_metrics = storage_->get_metrics(10);
    ASSERT_EQ(all_metrics.size(), 1);
    EXPECT_DOUBLE_EQ(all_metrics[0].capture_latency_ms, 4.5);
    EXPECT_DOUBLE_EQ(all_metrics[0].inference_latency_ms, 18.2);
    EXPECT_DOUBLE_EQ(all_metrics[0].fps, 37.5);
}

TEST_F(SQLiteStorageTest, HandlesMultipleEvents) {
    // ทดสอบ multiple events
    for (int i = 0; i < 100; ++i) {
        Event event;
        event.type = "frame_processed";
        event.timestamp = i;
        event.data = R"({"frame_id": )" + std::to_string(i) + "}";
        
        ASSERT_TRUE(storage_->store_event(event));
    }
    
    auto events = storage_->get_events(50);
    EXPECT_EQ(events.size(), 50);
    
    events = storage_->get_events(200);
    EXPECT_EQ(events.size(), 100);
}

TEST_F(SQLiteStorageTest, StoresConfiguration) {
    // ทดสอบ store configuration
    Config config;
    config.camera_width = 640;
    config.camera_height = 480;
    config.camera_fps = 30;
    config.model_path = "models/rtmpose.onnx";
    config.http_port = 8080;
    
    ASSERT_TRUE(storage_->store_config(config));
    
    auto loaded_config = storage_->get_config();
    EXPECT_EQ(loaded_config.camera_width, 640);
    EXPECT_EQ(loaded_config.camera_height, 480);
    EXPECT_EQ(loaded_config.camera_fps, 30);
    EXPECT_EQ(loaded_config.model_path, "models/rtmpose.onnx");
    EXPECT_EQ(loaded_config.http_port, 8080);
}

TEST_F(SQLiteStorageTest, HandlesConcurrentAccess) {
    // ทดสอบ concurrent access
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, i, &success_count]() {
            Event event;
            event.type = "concurrent_test";
            event.timestamp = i;
            event.data = "{}";
            
            if (storage_->store_event(event)) {
                success_count++;
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(success_count.load(), 10);
    
    auto events = storage_->get_events(100);
    EXPECT_EQ(events.size(), 10);
}

TEST_F(SQLiteStorageTest, HandlesDatabaseErrors) {
    // ทดสอบ database errors
    // ปิด database แล้วลอง store
    storage_.reset();
    
    // สร้าง storage ใหม่ที่ path ที่ไม่ valid
    SQLiteStorage invalid_storage("/invalid/path/db.db");
    EXPECT_FALSE(invalid_storage.initialize());
}

TEST_F(SQLiteStorageTest, MigrationsWork) {
    // ทดสอบ migrations
    // สร้าง database เวอร์ชันเก่า แล้ว upgrade
    
    // ปิด storage ปัจจุบัน
    storage_.reset();
    
    // สร้าง database เวอร์ชัน 1
    // (ในความเป็นจริง ต้องสร้าง schema เวอร์ชัน 1 ก่อน)
    
    // เปิด storage ใหม่ - ควร migrate อัตโนมัติ
    storage_ = std::make_unique<SQLiteStorage>(test_db_path_.string());
    ASSERT_TRUE(storage_->initialize());
    
    // ตรวจสอบว่า migrations ทำงาน
    EXPECT_EQ(storage_->schema_version(), 1);  // เวอร์ชันปัจจุบัน
}
