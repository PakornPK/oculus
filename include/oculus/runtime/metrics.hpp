#pragma once

#include <chrono>
#include <string>
#include <unordered_map>
#include <atomic>

namespace oculus {

class MetricsCollector {
public:
    void record_latency(const std::string& name, double ms);
    void record_counter(const std::string& name, int64_t value);
    void record_fps(double fps);

    double get_avg_latency(const std::string& name) const;
    int64_t get_counter(const std::string& name) const;
    double get_fps() const;

    void reset();

private:
    struct LatencyData {
        double total = 0.0;
        int64_t count = 0;
    };

    std::unordered_map<std::string, LatencyData> latencies_;
    std::unordered_map<std::string, std::atomic<int64_t>> counters_;
    std::atomic<double> fps_{0.0};
};

class ScopedTimer {
public:
    ScopedTimer(MetricsCollector& metrics, const std::string& name)
        : metrics_(metrics), name_(name),
          start_(std::chrono::steady_clock::now()) {}

    ~ScopedTimer() {
        auto end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start_).count();
        metrics_.record_latency(name_, ms);
    }

private:
    MetricsCollector& metrics_;
    std::string name_;
    std::chrono::steady_clock::time_point start_;
};

} // namespace oculus