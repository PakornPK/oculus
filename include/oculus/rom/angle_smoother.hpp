#pragma once

#include <deque>
#include <numeric>

namespace oculus {

class AngleSmoother {
public:
    explicit AngleSmoother(int window_size = 5) : window_size_(window_size) {}

    float smooth(float new_value) {
        buffer_.push_back(new_value);
        if (static_cast<int>(buffer_.size()) > window_size_) {
            buffer_.pop_front();
        }
        return average();
    }

    float average() const {
        if (buffer_.empty()) return 0.0f;
        float sum = std::accumulate(buffer_.begin(), buffer_.end(), 0.0f);
        return sum / static_cast<float>(buffer_.size());
    }

    void reset() { buffer_.clear(); }
    int size() const { return static_cast<int>(buffer_.size()); }

private:
    int window_size_;
    std::deque<float> buffer_;
};

} // namespace oculus