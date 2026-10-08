#include <gtest/gtest.h>
#include "oculus/storage/storage.hpp"

using namespace oculus;

TEST(StorageTest, OpenClose) {
    StorageConfig config;
    config.path = ":memory:";
    Storage storage(config);
    EXPECT_FALSE(storage.is_open());

    EXPECT_TRUE(storage.open());
    EXPECT_TRUE(storage.is_open());

    storage.close();
    EXPECT_FALSE(storage.is_open());
}

TEST(StorageTest, SaveLoadConfig) {
    StorageConfig config;
    config.path = ":memory:";
    Storage storage(config);
    storage.open();

    EXPECT_TRUE(storage.save_config("key1", "value1"));
    auto val = storage.load_config("key1", "default");
    EXPECT_EQ(val, "default");
}