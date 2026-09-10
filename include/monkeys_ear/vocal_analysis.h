#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace monkeys_ear {

// Shared, fixed-memory evidence passed between vocal subsystems.
// This is deliberately descriptive rather than prescriptive: target selection,
// resynthesis, dynamics and character stages decide how to use the evidence.
// LIVE producers must remain causal and allocation-free.
struct VocalAnalysisFrame {
    // Pitch evidence (cents in the engine's existing reference space).
    float observed_cents = 0.0f;
    float pitch_confidence = 0.0f;
    float center_cents = 0.0f;
    float drift_cents = 0.0f;
    float vibrato_cents = 0.0f;
    float vibrato_rate_hz = 0.0f;
    float vibrato_confidence = 0.0f;
    float transition_cents = 0.0f;
    float residual_cents = 0.0f;
    float motion_cents_per_second = 0.0f;

    // Continuous source mixture. These need not be mutually exclusive at the
    // producer, but normalize_source_weights() provides a stable mixture for
    // consumers that need one.
    float periodic_weight = 0.0f;
    float mixed_weight = 0.0f;
    float aperiodic_weight = 1.0f;

    // Phrase / articulation evidence.
    float onset = 0.0f;
    float release = 0.0f;
    float stability = 0.0f;
    float connection = 0.0f;
    float motion_intent = 0.0f;
    float target_age_seconds = 0.0f;

    // Cheap dynamics evidence shared with performance-aware processors.
    float peak = 0.0f;
    float rms = 0.0f;

    void sanitize() noexcept {
        observed_cents = finite_or_zero(observed_cents);
        center_cents = finite_or_zero(center_cents);
        drift_cents = finite_or_zero(drift_cents);
        vibrato_cents = finite_or_zero(vibrato_cents);
        transition_cents = finite_or_zero(transition_cents);
        residual_cents = finite_or_zero(residual_cents);
        motion_cents_per_second = finite_or_zero(motion_cents_per_second);
        vibrato_rate_hz = std::max(0.0f, finite_or_zero(vibrato_rate_hz));
        target_age_seconds = std::max(0.0f, finite_or_zero(target_age_seconds));
        peak = std::max(0.0f, finite_or_zero(peak));
        rms = std::max(0.0f, finite_or_zero(rms));

        pitch_confidence = unit(pitch_confidence);
        vibrato_confidence = unit(vibrato_confidence);
        periodic_weight = unit(periodic_weight);
        mixed_weight = unit(mixed_weight);
        aperiodic_weight = unit(aperiodic_weight);
        onset = unit(onset);
        release = unit(release);
        stability = unit(stability);
        connection = unit(connection);
        motion_intent = unit(motion_intent);
    }

    void normalize_source_weights() noexcept {
        periodic_weight = unit(periodic_weight);
        mixed_weight = unit(mixed_weight);
        aperiodic_weight = unit(aperiodic_weight);
        const float sum = periodic_weight + mixed_weight + aperiodic_weight;
        if (sum <= 1.0e-6f) {
            periodic_weight = 0.0f;
            mixed_weight = 0.0f;
            aperiodic_weight = 1.0f;
            return;
        }
        const float inv = 1.0f / sum;
        periodic_weight *= inv;
        mixed_weight *= inv;
        aperiodic_weight *= inv;
    }

    [[nodiscard]] float reconstructed_pitch_cents() const noexcept {
        return center_cents + drift_cents + vibrato_cents +
               transition_cents + residual_cents;
    }

private:
    static float finite_or_zero(float x) noexcept {
        return std::isfinite(x) ? x : 0.0f;
    }

    static float unit(float x) noexcept {
        return std::clamp(finite_or_zero(x), 0.0f, 1.0f);
    }
};

} // namespace monkeys_ear
