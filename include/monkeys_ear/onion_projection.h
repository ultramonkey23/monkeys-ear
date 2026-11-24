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
// Exact magnitude-squared response geometry for the production RBJ peaking
// cascade at the analyzer centers. Rebuild only when sample rate or Q changes.
struct FilterGeometry {
    std::array<Bands, kBands> weight{};
    float sample_rate = 0.0f;
    float q = 0.0f;

    void configure(float sr, float focus, const Bands& centers) noexcept {
        if (sr == sample_rate && focus == q) return;
        sample_rate = sr;
        q = focus;
        constexpr float two_pi = 6.28318530717958647692f;
        Bands sine{}, cosine{}, alpha{};
        for (std::size_t i = 0; i < kBands; ++i) {
            const float w = two_pi * std::clamp(centers[i], 30.0f, sr * 0.44f) / sr;
            sine[i] = std::sin(w);
            cosine[i] = std::cos(w);
            alpha[i] = sine[i] / (2.0f * focus);
        }
        for (std::size_t i = 0; i < kBands; ++i) {
            for (std::size_t j = 0; j < kBands; ++j) {
                const float d = cosine[i] - cosine[j];
                const float a = alpha[j] * sine[i];
                weight[i][j] = a * a / (d * d + a * a);
            }
        }
    }
};

struct Decision {
    Bands gain_db{};
    float identity = 1.0f;
    float conflict_before = 0.0f;
    float conflict_after = 0.0f;
};

inline Decision select(const Bands& self, const Bands& reference,
                       const Bands& proposal, const FilterGeometry* geometry = nullptr,
                       const Bands* harmonic_guard = nullptr) noexcept {
    Decision keep{};
    Bands overlap{};
    float norm_before = 0.0f;
    bool has_boost = false;
    bool has_cut = false;
    float max_cut_db = 0.0f;
    float max_boost_db = 0.0f;
    for (std::size_t i = 0; i < kBands; ++i) {
        overlap[i] = std::sqrt(std::max(0.0f, self[i] * reference[i]));
        keep.conflict_before += overlap[i];
        norm_before += self[i] * self[i];
        has_boost |= proposal[i] > 0.0f;
        has_cut |= proposal[i] < 0.0f;
        max_cut_db = std::min(max_cut_db, proposal[i]);
        max_boost_db = std::max(max_boost_db, proposal[i]);
    }
    keep.conflict_after = keep.conflict_before;
    if (!has_cut || keep.conflict_before <= 1e-4f) return keep;

    // The strongest unscaled candidate is the dynamic response budget, not
    // a per-filter allowance that silently multiplies through adjacent bands.
    const float min_amp = std::pow(10.0f, max_cut_db * 0.05f);
    const float max_amp = std::pow(10.0f, max_boost_db * 0.05f);
    Bands guarded_min{};
    if (geometry && harmonic_guard) {
        for (std::size_t i = 0; i < kBands; ++i) {
            const float guard = std::clamp((*harmonic_guard)[i], 0.0f, 1.0f);
            if (guard > 0.0f)
                guarded_min[i] = std::pow(10.0f, max_cut_db * (1.0f - guard) * 0.05f);
        }
    }

    // Try the authored proposal first, then a cuts-only alternative if a
    // Preserve boost prevents acceptance. Never apply an untested scale.
    // At most ten fixed-size projections; no dynamic memory or unbounded retries.
    constexpr float kIdentityFloor = 0.86f;
    for (int variant = 0; variant < (has_boost ? 2 : 1); ++variant) {
        float scale = 1.0f;
        Bands local_scale{};
        local_scale.fill(1.0f);
        for (int attempt = 0; attempt < 5; ++attempt) {
            Decision trial{};
            trial.conflict_before = keep.conflict_before;
            float dot = 0.0f;
            float norm_after = 0.0f;
            Bands amps{}, numerator_delta{}, denominator_delta{};
            std::array<std::size_t, kBands> active{};
            std::size_t active_count = 0;
            for (std::size_t i = 0; i < kBands; ++i) {
                const float base_db = variant == 0 ? proposal[i] : std::min(0.0f, proposal[i]);
                const float db = base_db * scale * (base_db < 0.0f ? local_scale[i] : 1.0f);
                trial.gain_db[i] = db;
                amps[i] = db == 0.0f ? 1.0f : std::pow(10.0f, db * 0.05f);
                if (db != 0.0f) {
                    active[active_count++] = i;
                    numerator_delta[i] = amps[i] - 1.0f;
                    denominator_delta[i] = 1.0f / amps[i] - 1.0f;
                }
            }
            float actual_min = 1.0f, actual_max = 1.0f;
            float protected_ratio = 1.0f;
            Bands protected_ratios{};
            protected_ratios.fill(1.0f);
            for (std::size_t i = 0; i < kBands; ++i) {
                float amp = amps[i];
                if (geometry) {
                    float power = 1.0f;
                    for (std::size_t k = 0; k < active_count; ++k) {
                        const auto j = active[k];
                        const float w = geometry->weight[i][j];
                        power *= (1.0f + numerator_delta[j] * w) /
                                 (1.0f + denominator_delta[j] * w);
                    }
                    amp = std::sqrt(power);
                    actual_min = std::min(actual_min, amp);
                    actual_max = std::max(actual_max, amp);
                    if (guarded_min[i] > 0.0f && amp < guarded_min[i] - 1e-5f) {
                        protected_ratios[i] = std::log(guarded_min[i]) / std::log(amp);
                        protected_ratio = std::min(protected_ratio, protected_ratios[i]);
                    }
                }
                const float projected = self[i] * amp;
                dot += self[i] * projected;
                norm_after += projected * projected;
                trial.conflict_after += overlap[i] * std::sqrt(amp);
            }
            // Use the measured response overshoot to propose the next scale.
            // It is only a proposal: the nonlinear response must pass again.
            if (actual_min < min_amp - 1e-5f || actual_max > max_amp + 1e-5f ||
                protected_ratio < 1.0f) {
                float ratio = 1.0f;
                if (actual_min < min_amp - 1e-5f)
                    ratio = std::min(ratio, std::log(min_amp) / std::log(actual_min));
                if (actual_max > max_amp + 1e-5f)
                    ratio = std::min(ratio, std::log(max_amp) / std::log(actual_max));
                if (ratio < 1.0f) {
                    scale *= std::clamp(ratio * 0.98f, 0.01f, 0.95f);
                } else if (attempt < 3 && geometry && harmonic_guard) {
                    // Prefer changing filters that actually touch the protected
                    // band. Distant masking corrections retain their authority.
                    for (std::size_t j = 0; j < kBands; ++j) {
                        if (trial.gain_db[j] >= 0.0f) continue;
                        float reduction = 1.0f;
                        for (std::size_t i = 0; i < kBands; ++i) {
                            if (protected_ratios[i] >= 1.0f) continue;
                            const float influence = std::sqrt(geometry->weight[i][j]);
                            reduction = std::min(reduction, 1.0f -
                                (1.0f - protected_ratios[i] * 0.90f) * influence);
                        }
                        local_scale[j] *= std::clamp(reduction, 0.01f, 1.0f);
                    }
                } else {
                    // A finite global backoff remains available if local
                    // protection does not pass the exact response check.
                    scale *= std::clamp(protected_ratio * 0.98f, 0.01f, 0.95f);
                }
                continue;
            }
            trial.identity = norm_before > 1e-8f && norm_after > 1e-8f
                ? std::clamp(dot / std::sqrt(norm_before * norm_after), 0.0f, 1.0f)
                : 1.0f;
            if (trial.identity >= kIdentityFloor &&
                trial.conflict_after < keep.conflict_before - 1e-6f) return trial;
            scale *= 0.72f;
        }
    }
    return keep;
}

} // namespace onion_projection
} // namespace monkeys_ear
