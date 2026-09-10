#include "monkeys_ear/latency_meter.h"
#include <vector>

namespace monkeys_ear {

LatencyMeter::LatencyMeter()
    : sample_rate_(48000.0f),
      block_size_(64),
      budget_us_(1333.33f),
      history_idx_(0),
      sample_count_(0),
      deadline_misses_(0),
      total_blocks_(0),
      running_sum_us_(0.0f) {
    history_us_.fill(0.0f);
}

void LatencyMeter::set_sample_rate(float sr) {
    sample_rate_ = sr > 0.0f ? sr : 48000.0f;
    budget_us_ = (static_cast<float>(block_size_) / sample_rate_) * 1000000.0f;
}

void LatencyMeter::set_block_size(size_t block_size) {
    block_size_ = block_size > 0 ? block_size : 64;
    budget_us_ = (static_cast<float>(block_size_) / sample_rate_) * 1000000.0f;
}

void LatencyMeter::start_block() {
    start_time_ = std::chrono::high_resolution_clock::now();
}

void LatencyMeter::end_block() {
    auto end_time = std::chrono::high_resolution_clock::now();
    float elapsed_us = std::chrono::duration<float, std::micro>(end_time - start_time_).count();

    total_blocks_++;
    if (elapsed_us > budget_us_) {
        deadline_misses_++;
    }

    running_sum_us_ -= history_us_[history_idx_];
    history_us_[history_idx_] = elapsed_us;
    running_sum_us_ += elapsed_us;

    history_idx_ = (history_idx_ + 1) % HISTORY_SIZE;
    if (sample_count_ < HISTORY_SIZE) {
        sample_count_++;
    }
}

void LatencyMeter::reset() {
    history_us_.fill(0.0f);
    history_idx_ = 0;
    sample_count_ = 0;
    deadline_misses_ = 0;
    total_blocks_ = 0;
    running_sum_us_ = 0.0f;
}

LatencyStats LatencyMeter::get_stats() const {
    LatencyStats stats{};
    stats.budget_us = budget_us_;
    stats.deadline_misses = deadline_misses_;
    stats.total_blocks = total_blocks_;

    if (sample_count_ == 0) {
        return stats;
    }

    stats.avg_us = running_sum_us_ / static_cast<float>(sample_count_);

    // Copy to sort for percentiles
    std::vector<float> sorted(history_us_.begin(), history_us_.begin() + sample_count_);
    std::sort(sorted.begin(), sorted.end());

    size_t idx50 = static_cast<size_t>(0.50f * (sample_count_ - 1));
    size_t idx95 = static_cast<size_t>(0.95f * (sample_count_ - 1));
    size_t idx99 = static_cast<size_t>(0.99f * (sample_count_ - 1));

    stats.p50_us = sorted[idx50];
    stats.p95_us = sorted[idx95];
    stats.p99_us = sorted[idx99];
    stats.max_us = sorted.back();

    if (budget_us_ > 0.0f) {
        stats.margin_percent = ((budget_us_ - stats.avg_us) / budget_us_) * 100.0f;
    }

    return stats;
}

} // namespace monkeys_ear
