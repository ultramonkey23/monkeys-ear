#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace monkeys_ear {
namespace onion_projection {

// Bounded projection of the current occupancy proxy, not a perceptual model.
// The caller owns evidence, musical intent, candidate generation and smoothing.
constexpr std::size_t kBands = 24;
using Bands = std::array<float, kBands>;
struct Decision {
    Bands gain_db{};
    float identity = 1.0f;
    float conflict_before = 0.0f;
    float conflict_after = 0.0f;
};

inline Decision select(const Bands& self, const Bands& reference,
                       const Bands& proposal) noexcept {
    Decision keep{};
    Bands overlap{};
    float norm_before = 0.0f;
    bool has_boost = false;
    bool has_cut = false;
    for (std::size_t i = 0; i < kBands; ++i) {
        overlap[i] = std::sqrt(std::max(0.0f, self[i] * reference[i]));
        keep.conflict_before += overlap[i];
        norm_before += self[i] * self[i];
        has_boost |= proposal[i] > 0.0f;
        has_cut |= proposal[i] < 0.0f;
    }
    keep.conflict_after = keep.conflict_before;
    if (!has_cut || keep.conflict_before <= 1e-4f) return keep;

    // Try the authored proposal first, then a cuts-only alternative if a
    // Preserve boost prevents acceptance. Never apply an untested scale.
    // At most ten fixed-size projections; no dynamic memory or retries.
    constexpr float kIdentityFloor = 0.86f;
    for (int variant = 0; variant < (has_boost ? 2 : 1); ++variant) {
        float scale = 1.0f;
        for (int attempt = 0; attempt < 5; ++attempt, scale *= 0.72f) {
            Decision trial{};
            trial.conflict_before = keep.conflict_before;
            float dot = 0.0f;
            float norm_after = 0.0f;
            for (std::size_t i = 0; i < kBands; ++i) {
                const float db = (variant == 0 ? proposal[i] : std::min(0.0f, proposal[i])) * scale;
                trial.gain_db[i] = db;
                const float amp = db == 0.0f ? 1.0f : std::pow(10.0f, db * 0.05f);
                const float projected = self[i] * amp;
                dot += self[i] * projected;
                norm_after += projected * projected;
                trial.conflict_after += overlap[i] * std::sqrt(amp);
            }
            trial.identity = norm_before > 1e-8f && norm_after > 1e-8f
                ? std::clamp(dot / std::sqrt(norm_before * norm_after), 0.0f, 1.0f)
                : 1.0f;
            if (trial.identity >= kIdentityFloor &&
                trial.conflict_after < keep.conflict_before - 1e-6f) return trial;
        }
    }
    return keep;
}

} // namespace onion_projection
} // namespace monkeys_ear
