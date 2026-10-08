#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <chrono>

namespace oculus {

template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(size_t max_size) : max_size_(max_size) {}

    bool try_push(T item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= max_size_) {
            return false;
        }
        queue_.push_back(std::move(item));
        cv_.notify_one();
        return true;
    }

    void push_overwrite(T item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= max_size_) {
            queue_.pop_front();
        }
        queue_.push_back(std::move(item));
        cv_.notify_one();
    }

    std::optional<T> try_pop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return std::nullopt;
        T item = std::move(queue_.front());
        queue_.pop_front();
        return item;
    }

    template <typename Rep, typename Period>
    std::optional<T> wait_pop(const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            if (cv_.wait_for(lock, timeout) == std::cv_status::timeout) {
                return std::nullopt;
            }
        }
        if (queue_.empty()) return std::nullopt;
        T item = std::move(queue_.front());
        queue_.pop_front();
        return item;
    }

    std::optional<T> pop_front() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return std::nullopt;
        T item = std::move(queue_.front());
        queue_.pop_front();
        return item;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.clear();
    }

    size_t max_size() const { return max_size_; }

private:
    size_t max_size_;
    std::deque<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};

} // namespace oculus