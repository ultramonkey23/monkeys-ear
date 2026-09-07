#pragma once

#include "monkeys_ear/types.h"

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
};

struct VocalExpressionMetrics {
    float tracked_hz = 0.0f;
    float confidence = 0.0f;
    float correction_cents = 0.0f;
    float voiced_mix = 0.0f;
    float residual_mix = 0.0f;
    bool voiced = false;
};

class AudioInputProcessor {
public:
    static constexpr size_t RING_BUFFER_SIZE = 8192;

    AudioInputProcessor();
    void set_sample_rate(float sr);
    void set_gain(float gain_db);     // -24dB to +24dB
    void set_mix(float mix);         // 0.0 (synth only) to 1.0 (mic only)
    void set_highpass_enabled(bool en);
    void set_vocal_controls(const VocalExpressionControls& controls) { vocal_controls_ = controls; }
    void reset();

    // Process one input frame: updates ring buffer, measures levels, returns blended signal
    float process_sample(float mic_in, float synth_in);
    float process_vocal_sample(float input, float tracked_hz, float confidence);

    float get_rms_level() const { return rms_level_; }
    float get_peak_level() const { return peak_level_; }
    float read_ring_buffer(size_t lag) const { return ring_buffer_.read(lag); }
    const VocalExpressionMetrics& vocal_metrics() const { return vocal_metrics_; }

private:
    float sample_rate_;
    float gain_linear_;
    float mix_;
    bool highpass_enabled_;

    LockFreeRingBuffer<float, RING_BUFFER_SIZE> ring_buffer_;

    // DC-block / highpass filter
    float hp_x1_;
    float hp_y1_;

    // Envelope followers for live feedback
    float rms_accumulator_;
    float rms_level_;
    float peak_level_;
    size_t rms_count_;

    VocalExpressionControls vocal_controls_{};
    VocalExpressionMetrics vocal_metrics_{};
    float grain_phase_ = 0.0f;
    float correction_cents_ = 0.0f;
    float pitch_fast_cents_ = 0.0f;
    float pitch_slow_cents_ = 0.0f;
    float source_envelope_ = 0.0f;
    float shifted_envelope_ = 0.0f;
    float residual_low_ = 0.0f;
};

} // namespace monkeys_ear
