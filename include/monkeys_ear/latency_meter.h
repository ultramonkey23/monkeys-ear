#pragma once

#include <chrono>
#include <array>
#include <algorithm>
#include <cstdint>

namespace monkeys_ear {

struct LatencyStats {
    float avg_us;
    float p50_us;
    float p95_us;
    float p99_us;
    float max_us;
    float budget_us;
    float margin_percent;
    uint64_t deadline_misses;
    uint64_t total_blocks;
};

class LatencyMeter {
public:
    static constexpr size_t HISTORY_SIZE = 1024;

    LatencyMeter();
    void set_sample_rate(float sr);
    void set_block_size(size_t block_size);

    void start_block();
    void end_block();
    void reset();

    LatencyStats get_stats() const;

private:
    float sample_rate_;
    size_t block_size_;
    float budget_us_;

    std::chrono::high_resolution_clock::time_point start_time_;
    std::array<float, HISTORY_SIZE> history_us_;
    size_t history_idx_;
    size_t sample_count_;
    uint64_t deadline_misses_;
    uint64_t total_blocks_;
    float running_sum_us_;
};

} // namespace monkeys_ear
