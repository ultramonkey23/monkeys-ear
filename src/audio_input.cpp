#include "monkeys_ear/audio_input.h"
#include <cmath>

namespace monkeys_ear {

AudioInputProcessor::AudioInputProcessor()
    : sample_rate_(48000.0f),
      gain_linear_(1.0f),
      mix_(0.0f),
      highpass_enabled_(true),
      hp_x1_(0.0f),
      hp_y1_(0.0f),
      rms_accumulator_(0.0f),
      rms_level_(0.0f),
      peak_level_(0.0f),
      rms_count_(0) {
}

void AudioInputProcessor::set_sample_rate(float sr) {
    sample_rate_ = std::max(1000.0f, sr);
}

void AudioInputProcessor::set_gain(float gain_db) {
    float clamped_db = clamp(gain_db, -24.0f, 24.0f);
    gain_linear_ = std::pow(10.0f, clamped_db / 20.0f);
}

void AudioInputProcessor::set_mix(float mix) {
    mix_ = clamp(mix, 0.0f, 1.0f);
}

void AudioInputProcessor::set_highpass_enabled(bool en) {
    highpass_enabled_ = en;
}

void AudioInputProcessor::reset() {
    ring_buffer_.reset();
    hp_x1_ = 0.0f;
    hp_y1_ = 0.0f;
    rms_accumulator_ = 0.0f;
    rms_level_ = 0.0f;
    peak_level_ = 0.0f;
    rms_count_ = 0;
}

float AudioInputProcessor::process_sample(float mic_in, float synth_in) {
    // Apply preamp gain
    float conditioned = mic_in * gain_linear_;

    // 20Hz DC-blocking highpass filter: y[n] = x[n] - x[n-1] + R * y[n-1]
    if (highpass_enabled_) {
        float r = 1.0f - (TWO_PI * 20.0f / sample_rate_);
        float hp_out = conditioned - hp_x1_ + r * hp_y1_;
        hp_x1_ = conditioned;
        hp_y1_ = hp_out;
        conditioned = hp_out;
    }

    // Soft clip any extreme mic spikes
    conditioned = fast_tanh(conditioned);

    // Push into causal ring buffer for analysis/playback
    ring_buffer_.push(conditioned);

    // Track level metrics
    float abs_val = std::abs(conditioned);
    if (abs_val > peak_level_) {
        peak_level_ = abs_val;
    } else {
        peak_level_ *= 0.9995f; // Fast peak decay
    }

    rms_accumulator_ += conditioned * conditioned;
    rms_count_++;
    if (rms_count_ >= 512) {
        rms_level_ = std::sqrt(rms_accumulator_ / 512.0f);
        rms_accumulator_ = 0.0f;
        rms_count_ = 0;
    }

    // Blend mic input with synth source
    float blended = lerp(synth_in, conditioned, mix_);
    return sanitize(blended);
}

} // namespace monkeys_ear
