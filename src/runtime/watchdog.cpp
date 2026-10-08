#include "oculus/runtime/watchdog.hpp"

namespace oculus {

Watchdog::Watchdog() = default;

Watchdog::~Watchdog() { stop(); }

void Watchdog::start(std::chrono::seconds timeout) {
    if (running_) return;
    timeout_ = timeout;
    running_ = true;
    alive_ = true;
    last_feed_ = std::chrono::steady_clock::now().time_since_epoch().count();

    thread_ = std::thread([this]() {
        while (running_) {
            auto now = std::chrono::steady_clock::now().time_since_epoch().count();
            if (now - last_feed_.load() > timeout_.count() * 1000000000LL) {
                alive_ = false;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
}

void Watchdog::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void Watchdog::feed() {
    last_feed_ = std::chrono::steady_clock::now().time_since_epoch().count();
    alive_ = true;
}

bool Watchdog::is_alive() const { return alive_; }

} // namespace oculus