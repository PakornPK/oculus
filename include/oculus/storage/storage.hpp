#pragma once

#include <string>
#include <vector>
#include <functional>

namespace oculus {

struct StorageConfig {
    std::string path = "/data/oculus.db";
    bool auto_backup = true;
};

class Storage {
public:
    explicit Storage(const StorageConfig& config);
    ~Storage();

    bool open();
    void close();
    bool is_open() const;

    bool save_config(const std::string& key, const std::string& value);
    std::string load_config(const std::string& key, const std::string& default_value = "");

private:
    StorageConfig config_;
    bool open_ = false;
};

} // namespace oculus