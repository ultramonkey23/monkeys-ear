#pragma once

#include "monkeys_ear/types.h"
#include "monkeys_ear/vocal_analysis.h"
#include "monkeys_ear/vocal_target.h"

namespace monkeys_ear {

// LIVE vocal semantics: correction is optional, never a claim of exact pitch.
// The voiced stage is causal and pitch-synchronous; unvoiced/noisy material
// stays direct except for the explicitly capped residual character layer.
struct VocalExpressionControls {
    bool enabled = false;
    float correction_strength = 0.0f;
    float drift_retention = 1.0f;
    float vibrato_retention = 1.0f;
    float transition = 0.5f;
    float formant_repair = 0.0f;
    float spectral_residual_mix = 0.0f;
    float character = 0.0f;
    float mix = 1.0f;
    float sequential_stage_mix = 0.0f;
    float aperiodic_protection = 1.0f;
    VocalTargetControls target{};
};

enum class VocalSourceType : uint8_t { Aperiodic, Mixed, Periodic };

struct VocalExpressionMetrics {
    float tracked_hz = 0.0f;
    float confidence = 0.0f;
    float correction_cents = 0.0f;
    float voiced_mix = 0.0f;
    float residual_mix = 0.0f;
    float periodic_mix = 0.0f;
    float aperiodic_mix = 1.0f;
    float selected_target_cents = 0.0f;
    float target_trajectory_cents = 0.0f;
    int selected_degree = -1;
    VocalSourceType source_type = VocalSourceType::Aperiodic;
    bool voiced = false;
};

class AudioInputProcessor {
public:
    static constexpr size_t RING_BUFFER_SIZE = 8192;

    AudioInputProcessor();
    void set_sample_rate(float sr);
    void set_gain(float gain_db);
    void set_mix(float mix);
    void set_highpass_enabled(bool en);
    void set_vocal_controls(const VocalExpressionControls& controls);
    void reset();

    float process_sample(float mic_in, float synth_in);
    float process_vocal_sample(float input, float tracked_hz, float confidence);

    float get_rms_level() const { return rms_level_; }
    float get_peak_level() const { return peak_level_; }
    float read_ring_buffer(size_t lag) const { return ring_buffer_.read(lag); }
    const VocalExpressionMetrics& vocal_metrics() const { return vocal_metrics_; }
    // Shared evidence is descriptive. Consumers must not reinterpret motion
    // residual as proven vibrato until a classifier establishes that evidence.
    const VocalAnalysisFrame& vocal_analysis() const { return vocal_analysis_; }

private:
    float sample_rate_;
    float gain_linear_;
    float mix_;
    bool highpass_enabled_;

    LockFreeRingBuffer<float, RING_BUFFER_SIZE> ring_buffer_;
    LockFreeRingBuffer<float, RING_BUFFER_SIZE> vocal_stage_ring_buffer_;

    float hp_x1_;
    float hp_y1_;
    float rms_accumulator_;
    float rms_level_;
    float peak_level_;
    size_t rms_count_;

    VocalExpressionControls vocal_controls_{};
    VocalExpressionMetrics vocal_metrics_{};
    VocalAnalysisFrame vocal_analysis_{};
    float grain_phase_ = 0.0f;
    float secondary_grain_phase_ = 0.0f;
    float correction_cents_ = 0.0f;
    float pitch_fast_cents_ = 0.0f;
    float pitch_slow_cents_ = 0.0f;
    float source_envelope_ = 0.0f;
    float shifted_envelope_ = 0.0f;
    float residual_low_ = 0.0f;
    float analysis_low_ = 0.0f;
    float periodic_energy_ = 0.0f;
    float aperiodic_energy_ = 0.0f;
    float onset_fast_ = 0.0f;
    float onset_slow_ = 0.0f;
    VocalTargetEngine vocal_target_{};
};

} // namespace monkeys_ear
