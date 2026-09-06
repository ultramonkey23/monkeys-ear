#include "monkeys_ear/safety_limiter.h"
#include <cmath>

namespace monkeys_ear {

SafetyLimiter::SafetyLimiter()
    : ceiling_linear_(0.98f),
      threshold_(0.80f) {
}

void SafetyLimiter::set_ceiling(float ceiling_db) {
    float db = clamp(ceiling_db, -6.0f, 0.0f);
    ceiling_linear_ = std::pow(10.0f, db / 20.0f);
    threshold_ = ceiling_linear_ * 0.85f;
}

void SafetyLimiter::reset() {
    // Zero latency stateless soft-knee
}

void SafetyLimiter::process(float& left, float& right) {
    left = sanitize(left);
    right = sanitize(right);

    float max_val = std::max(std::abs(left), std::abs(right));

    if (max_val > threshold_) {
        // Soft rational compression above threshold:
        // y = threshold + (ceiling - threshold) * tanh((x - threshold) / (ceiling - threshold))
        float headroom = ceiling_linear_ - threshold_;
        float over = max_val - threshold_;
        float compressed_peak = threshold_ + headroom * fast_tanh(over / headroom);

        float gain_reduction = compressed_peak / max_val;
        left *= gain_reduction;
        right *= gain_reduction;
    }

    // Absolute brickwall clamp as final guarantee
    left = clamp(left, -ceiling_linear_, ceiling_linear_);
    right = clamp(right, -ceiling_linear_, ceiling_linear_);
}

} // namespace monkeys_ear
