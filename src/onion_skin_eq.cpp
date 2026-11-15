#include "monkeys_ear/onion_skin_eq.h"
#include <algorithm>
#include <cmath>

namespace monkeys_ear {

namespace {

inline void compute_bandpass(float sr, float freq_hz, float q,
                             float& b0, float& b1, float& b2, float& a1, float& a2) noexcept {
    const float w0 = TWO_PI * clamp(freq_hz, 30.0f, sr * 0.44f) / std::max(1.0f, sr);
    const float cos_w = std::cos(w0);
    const float alpha = std::sin(w0) / (2.0f * std::max(0.2f, q));
    const float a0 = 1.0f + alpha;
    const float inv_a0 = 1.0f / a0;
    b0 = alpha * inv_a0;
    b1 = 0.0f;
    b2 = -alpha * inv_a0;
    a1 = (-2.0f * cos_w) * inv_a0;
    a2 = (1.0f - alpha) * inv_a0;
}

inline void compute_peaking(float sr, float freq_hz, float q, float gain_db,
                            float& b0, float& b1, float& b2, float& a1, float& a2) noexcept {
    const float safe_db = clamp(gain_db, -14.0f, 6.0f);
    if (std::abs(safe_db) < 0.03f) {
        b0 = 1.0f; b1 = 0.0f; b2 = 0.0f; a1 = 0.0f; a2 = 0.0f;
        return;
    }
    const float A = std::pow(10.0f, safe_db * 0.025f);
    const float w0 = TWO_PI * clamp(freq_hz, 30.0f, sr * 0.44f) / std::max(1.0f, sr);
    const float cos_w = std::cos(w0);
    const float alpha = std::sin(w0) / (2.0f * std::max(0.2f, q));
    const float alpha_mul_A = alpha * A;
    const float alpha_div_A = alpha / A;
    const float a0 = 1.0f + alpha_div_A;
    const float inv_a0 = 1.0f / a0;
    b0 = (1.0f + alpha_mul_A) * inv_a0;
    b1 = (-2.0f * cos_w) * inv_a0;
    b2 = (1.0f - alpha_mul_A) * inv_a0;
    a1 = b1;
    a2 = (1.0f - alpha_div_A) * inv_a0;
}

} // namespace

OnionSkinEQ::OnionSkinEQ() noexcept {
    set_sample_rate(48000.0f);
    reset();
}

void OnionSkinEQ::set_sample_rate(float sr) noexcept {
    sample_rate_ = std::max(8000.0f, sr);
    atk_coeff_ = 1.0f - std::exp(-1.0f / (0.008f * sample_rate_));
    rel_coeff_ = 1.0f - std::exp(-1.0f / (0.095f * sample_rate_));
    setup_analyzers();
}

void OnionSkinEQ::set_controls(const OnionSkinControls& controls) noexcept {
    controls_ = controls;
}

void OnionSkinEQ::reset() noexcept {
    for (size_t i = 0; i < kNumBands; ++i) {
        self_bp_[i].reset();
        ref_bp_[i].reset();
        carve_eq_[i].reset();
        update_peaking_coeffs(i, 0.0f);
    }
    self_env_.fill(0.0f);
    ref_env_.fill(0.0f);
    peer_occ_.fill(0.0f);
    target_db_.fill(0.0f);
    current_db_.fill(0.0f);
    metrics_ = OnionSkinMetrics{};
    fast_env_ = 0.0f;
    slow_env_ = 0.0f;
    control_counter_ = 0;
}

void OnionSkinEQ::set_peer_occupancy(const std::array<float, kNumBands>& peer_occ, float peer_weight) noexcept {
    peer_occ_ = peer_occ;
    peer_weight_ = clamp(peer_weight, 0.0f, 1.0f);
}

void OnionSkinEQ::setup_analyzers() noexcept {
    for (size_t i = 0; i < kNumBands; ++i) {
        compute_bandpass(sample_rate_, kBandCentersHz[i], 1.65f,
                         self_bp_[i].b0, self_bp_[i].b1, self_bp_[i].b2, self_bp_[i].a1, self_bp_[i].a2);
        ref_bp_[i].b0 = self_bp_[i].b0;
        ref_bp_[i].b1 = self_bp_[i].b1;
        ref_bp_[i].b2 = self_bp_[i].b2;
        ref_bp_[i].a1 = self_bp_[i].a1;
        ref_bp_[i].a2 = self_bp_[i].a2;
        update_peaking_coeffs(i, current_db_[i]);
    }
}

void OnionSkinEQ::update_peaking_coeffs(size_t band, float gain_db) noexcept {
    compute_peaking(sample_rate_, kBandCentersHz[band], 1.45f, gain_db,
                    carve_eq_[band].b0, carve_eq_[band].b1, carve_eq_[band].b2,
                    carve_eq_[band].a1, carve_eq_[band].a2);
}

void OnionSkinEQ::evaluate_projection_gate() noexcept {
    const float amount = clamp(controls_.amount, 0.0f, 1.0f);
    const float sens = clamp(controls_.sensitivity, 0.0f, 1.0f);
    const float low_prot = clamp(controls_.low_protect, 0.0f, 1.0f);
    const float trans_prot = clamp(controls_.transient_protect, 0.0f, 1.0f);

    // Compute transient hold factor [0..1]: high when fast envelope jumps cleanly above slow envelope
    const float trans_ratio = (fast_env_ - slow_env_) / (slow_env_ + 0.015f);
    const float transient_hold = clamp(trans_ratio * 1.4f, 0.0f, 1.0f) * trans_prot;
    metrics_.transient_hold = transient_hold;

    // Low-harmonic band protections (bands 0 = 140 Hz, 1 = 320 Hz, 2 = 720 Hz)
    const std::array<float, kNumBands> band_protect{{
        low_prot * 0.92f,
        low_prot * 0.72f,
        low_prot * 0.28f,
        0.0f,
        0.0f,
        0.0f
    }};

    std::array<float, kNumBands> proposed_db{};
    float conflict_before = 0.0f;

    for (size_t i = 0; i < kNumBands; ++i) {
        const float self_o = clamp(self_env_[i] * 2.8f, 0.0f, 1.0f);
        const float local_ref_o = clamp(ref_env_[i] * 2.8f, 0.0f, 1.0f);
        const float ref_o = clamp(std::max(local_ref_o, peer_occ_[i] * peer_weight_), 0.0f, 1.0f);

        metrics_.self_occupancy[i] = self_o;
        metrics_.ref_occupancy[i] = ref_o;

        const float overlap = std::sqrt(std::max(0.0f, self_o * ref_o));
        conflict_before += overlap;

        const float thresh = lerp(0.28f, 0.04f, sens);
        const float excess = std::max(0.0f, overlap - thresh);
        if (excess <= 1e-4f || !controls_.enabled || amount <= 1e-4f) {
            proposed_db[i] = 0.0f;
            continue;
        }

        float intent_scale = 0.55f;
        if (controls_.intent == OnionSkinIntent::Yield) {
            // Yield strongly where reference has clear competing presence
            const float dominance = clamp((ref_o + 0.05f) / (self_o + ref_o + 0.08f), 0.35f, 1.25f);
            intent_scale = 1.05f * dominance;
        } else if (controls_.intent == OnionSkinIntent::Preserve) {
            // Preserve own presence; only lightly notch upper-mid harshness
            intent_scale = (i >= 3) ? 0.45f : 0.14f;
        }

        const float protect = clamp((1.0f - band_protect[i]) * (1.0f - transient_hold * 0.8f), 0.05f, 1.0f);
        float cut_db = -11.5f * amount * intent_scale * protect * clamp(excess * 3.0f, 0.0f, 1.0f);
        if (controls_.intent == OnionSkinIntent::Preserve && i <= 1 && self_o > 0.05f) {
            // Optional micro-anchor lift when preserving core body against dense peer
            cut_db = std::max(cut_db, 1.2f * amount * overlap);
        }
        proposed_db[i] = clamp(cut_db, -12.0f, 2.0f);
    }

    // Projection Gate: check identity cosine similarity before vs after proposed gains.
    // Enforce cosine >= 0.86 so Onion Skin EQ never hollows out core timbre identity.
    float scale = 1.0f;
    float final_cos = 1.0f;
    float final_conflict_after = conflict_before;

    for (int iter = 0; iter < 5; ++iter) {
        float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f, c_after = 0.0f;
        for (size_t i = 0; i < kNumBands; ++i) {
            const float s = metrics_.self_occupancy[i] + 1e-4f;
            const float r = metrics_.ref_occupancy[i] + 1e-4f;
            const float lin = std::pow(10.0f, (proposed_db[i] * scale) * 0.05f);
            const float s_carved = s * lin;
            dot += s * s_carved;
            norm_a += s * s;
            norm_b += s_carved * s_carved;
            c_after += std::sqrt(std::max(0.0f, s_carved * r));
        }
        final_cos = (norm_a > 1e-8f && norm_b > 1e-8f) ? (dot / std::sqrt(norm_a * norm_b)) : 1.0f;
        final_conflict_after = c_after;
        if (final_cos >= 0.86f || scale <= 0.15f) {
            break;
        }
        scale *= 0.72f;
    }

    metrics_.identity_cosine = clamp(final_cos, 0.0f, 1.0f);
    metrics_.conflict_reduction = (conflict_before > 1e-4f)
        ? clamp((conflict_before - final_conflict_after) / conflict_before, 0.0f, 1.0f)
        : 0.0f;

    for (size_t i = 0; i < kNumBands; ++i) {
        target_db_[i] = controls_.enabled ? (proposed_db[i] * scale) : 0.0f;
    }
}

float OnionSkinEQ::process_sample(float input, float local_reference) noexcept {
    const float abs_in = std::abs(input);
    const float f_coeff = abs_in > fast_env_ ? atk_coeff_ * 2.5f : rel_coeff_ * 4.0f;
    const float s_coeff = abs_in > slow_env_ ? atk_coeff_ * 0.25f : rel_coeff_ * 0.5f;
    fast_env_ += (abs_in - fast_env_) * clamp(f_coeff, 0.0f, 0.5f);
    slow_env_ += (abs_in - slow_env_) * clamp(s_coeff, 0.0f, 0.5f);

    // Measure ERB bands unconditionally so telemetry & Ecosystem SpectralOccupancy always stay live
    for (size_t i = 0; i < kNumBands; ++i) {
        const float sb = std::abs(self_bp_[i].process(input));
        const float rb = std::abs(ref_bp_[i].process(local_reference));
        const float sc = sb > self_env_[i] ? atk_coeff_ : rel_coeff_;
        const float rc = rb > ref_env_[i] ? atk_coeff_ : rel_coeff_;
        self_env_[i] += (sb - self_env_[i]) * sc;
        ref_env_[i] += (rb - ref_env_[i]) * rc;
    }

    if (++control_counter_ >= 32) {
        control_counter_ = 0;
        evaluate_projection_gate();
        for (size_t i = 0; i < kNumBands; ++i) {
            current_db_[i] += (target_db_[i] - current_db_[i]) * 0.28f;
            metrics_.band_carve_db[i] = current_db_[i];
            update_peaking_coeffs(i, current_db_[i]);
        }
    }

    if (!controls_.enabled) {
        return input;
    }

    float y = input;
    for (size_t i = 0; i < kNumBands; ++i) {
        if (std::abs(current_db_[i]) > 0.04f) {
            y = carve_eq_[i].process(y);
        }
    }
    return sanitize(y);
}

} // namespace monkeys_ear
