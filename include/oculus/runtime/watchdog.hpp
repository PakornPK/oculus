#pragma once

#include <atomic>
#include <thread>
#include <chrono>
#include <functional>

namespace oculus {

class Watchdog {
public:
    Watchdog();
    ~Watchdog();

    void start(std::chrono::seconds timeout);
    void stop();
    void feed();
    bool is_alive() const;

private:
    std::atomic<bool> running_{false};
    std::atomic<bool> alive_{false};
    std::atomic<int64_t> last_feed_{0};
    std::chrono::seconds timeout_{30};
    std::thread thread_;
};

} // namespace oculus