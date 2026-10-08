#include "oculus/runtime/metrics.hpp"

namespace oculus {

void MetricsCollector::record_latency(const std::string& name, double ms) {
    auto& data = latencies_[name];
    data.total += ms;
    data.count++;
}

void MetricsCollector::record_counter(const std::string& name, int64_t value) {
    counters_[name].store(value);
}

void MetricsCollector::record_fps(double fps) {
    fps_.store(fps);
}

double MetricsCollector::get_avg_latency(const std::string& name) const {
    auto it = latencies_.find(name);
    if (it == latencies_.end() || it->second.count == 0) return 0.0;
    return it->second.total / static_cast<double>(it->second.count);
}

int64_t MetricsCollector::get_counter(const std::string& name) const {
    auto it = counters_.find(name);
    if (it == counters_.end()) return 0;
    return it->second.load();
}

double MetricsCollector::get_fps() const {
    return fps_.load();
}

void MetricsCollector::reset() {
    latencies_.clear();
    counters_.clear();
    fps_.store(0.0);
}

} // namespace oculus