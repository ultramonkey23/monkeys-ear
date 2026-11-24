#pragma once

#include "monkeys_ear/types.h"
#include <array>

namespace monkeys_ear {

enum class OnionSkinIntent : int {
    Equal = 0,
    Yield = 1,
    Preserve = 2
};

struct OnionSkinControls {
    bool enabled = false;
    OnionSkinIntent intent = OnionSkinIntent::Yield;
    float amount = 0.65f;            // Carve authority [0, 1]
    float sensitivity = 0.55f;       // Conflict detection sensitivity [0, 1]
    float low_protect = 0.70f;       // Protects fundamental & sub bands [0, 1]
    float transient_protect = 0.60f; // Holds carve back during transients [0, 1]
    float harmonic_protect = 0.0f;   // Pitch-informed f0..3*f0 harmonic protection [0, 1]
    float tilt_db = 0.0f;            // Spectral tilt [-6, +6] dB across ERB spectrum
    float q_focus = 1.85f;           // Peaking surgical Q [0.85, 4.50]
    float air_db = 0.0f;             // Upper-air shimmer shelf >= 4.2 kHz [-6, +6] dB
    std::array<float, 24> manual_db{}; // User direct per-band surgical EQ [-12, +12] dB
};

struct OnionSkinMetrics {
    std::array<float, 24> self_occupancy{};
    std::array<float, 24> ref_occupancy{};
    std::array<float, 24> band_carve_db{};
    std::array<float, 24> user_db{};
    std::array<float, 24> total_eq_db{};
    float identity_cosine = 1.0f;
    float conflict_reduction = 0.0f;
    float transient_hold = 0.0f;
};

// Zero-latency, zero-allocation, lock-free 24-band ERB Onion Skin Dynamic + Surgical EQ.
// Analyzes self and reference/peer spectral occupancy across 24 Glasberg-Moore ERB auditory critical bands
// spanning 40 Hz - 9.6 kHz, enforces low-harmonic and transient protection plus a cosine >= 0.86
// Projection Gate on dynamic carving, sums user 24-band surgical sculpt + Tilt + Air shelf,
// and applies smooth biquad EQ filtering per sample.
class OnionSkinEQ {
public:
    static constexpr size_t kNumBands = 24;
    static constexpr std::array<float, kNumBands> kBandCentersHz{{
        40.0f, 72.0f, 108.0f, 150.0f, 198.0f, 255.0f,
        322.0f, 400.0f, 492.0f, 600.0f, 728.0f, 878.0f,
        1055.0f, 1265.0f, 1510.0f, 1800.0f, 2145.0f, 2550.0f,
        3030.0f, 3600.0f, 4275.0f, 5150.0f, 6600.0f, 9600.0f
    }};

    OnionSkinEQ() noexcept;

    void set_sample_rate(float sr) noexcept;
    void set_controls(const OnionSkinControls& controls) noexcept;
    void reset() noexcept;

    // Set external peer spectral occupancy (24 ERB normalized bands [0..1]) and blend weight.
    void set_peer_occupancy(const std::array<float, kNumBands>& peer_occ, float peer_weight) noexcept;
    // Set tracked self pitch (Hz) & confidence [0..1] for harmonic/fundamental guard.
    void set_self_pitch(float hz, float confidence) noexcept;

    // Process one audio sample using local reference (e.g. harmonic/weight sidechain) + peer evidence.
    float process_sample(float input, float local_reference) noexcept;

    const OnionSkinMetrics& metrics() const noexcept { return metrics_; }
    const std::array<float, kNumBands>& self_occupancy() const noexcept { return metrics_.self_occupancy; }

private:
    struct Biquad {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;

        void reset() noexcept { z1 = 0.0f; z2 = 0.0f; }
        float process(float x) noexcept {
            float y = b0 * x + z1;
            z1 = sanitize(b1 * x - a1 * y + z2);
            z2 = sanitize(b2 * x - a2 * y);
            return sanitize(y);
        }
    };

    float sample_rate_ = 48000.0f;
    OnionSkinControls controls_{};
    OnionSkinMetrics metrics_{};

    std::array<Biquad, kNumBands> self_bp_{};
    std::array<Biquad, kNumBands> ref_bp_{};
    std::array<Biquad, kNumBands> carve_eq_{};

    std::array<float, kNumBands> self_env_{};
    std::array<float, kNumBands> ref_env_{};
    std::array<float, kNumBands> peer_occ_{};
    float peer_weight_ = 0.0f;
    float self_pitch_hz_ = 0.0f;
    float self_pitch_confidence_ = 0.0f;

    std::array<float, kNumBands> target_db_{};
    std::array<float, kNumBands> current_db_{};

    float fast_env_ = 0.0f;
    float slow_env_ = 0.0f;
    float atk_coeff_ = 0.05f;
    float rel_coeff_ = 0.002f;
    int control_counter_ = 0;

    void setup_analyzers() noexcept;
    void update_peaking_coeffs(size_t band, float gain_db) noexcept;
    void evaluate_projection_gate() noexcept;
};

} // namespace monkeys_ear
