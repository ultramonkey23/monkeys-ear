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
    vocal_stage_ring_buffer_.reset();
    hp_x1_ = 0.0f;
    hp_y1_ = 0.0f;
    rms_accumulator_ = 0.0f;
    rms_level_ = 0.0f;
    peak_level_ = 0.0f;
    rms_count_ = 0;
    grain_phase_ = secondary_grain_phase_ = correction_cents_ = pitch_fast_cents_ = pitch_slow_cents_ = 0.0f;
    source_envelope_ = shifted_envelope_ = residual_low_ = analysis_low_ = periodic_energy_ = aperiodic_energy_ = 0.0f;
    vocal_metrics_ = {};
}

float AudioInputProcessor::process_vocal_sample(float input, float tracked_hz, float confidence) {
    const auto& c = vocal_controls_;
    input = sanitize(input);
    if (!c.enabled) { vocal_metrics_ = {}; return input; }

    const float hz = clamp(tracked_hz, 55.0f, 900.0f);
    const float conf = clamp(confidence, 0.0f, 1.0f);
    const bool voiced = tracked_hz >= 55.0f && tracked_hz <= 900.0f && conf > 0.35f;
    const float confidence_mix = voiced ? clamp((conf - 0.35f) / 0.55f, 0.0f, 1.0f) : 0.0f;

    // V2: classify a frame as periodic / mixed / aperiodic continuously.  This
    // is intentionally a confidence-weighted estimate, not an overconfident
    // voiced/unvoiced verdict.  The high-passed remainder is a cheap causal
    // proxy for breath, sibilance, and consonant/transient participation.
    const float analysis_alpha = 1.0f - std::exp(-TWO_PI * 1200.0f / sample_rate_);
    analysis_low_ += (input - analysis_low_) * analysis_alpha;
    const float analysis_alpha_energy = 1.0f - std::exp(-1.0f / (0.012f * sample_rate_));
    periodic_energy_ += (std::abs(analysis_low_) - periodic_energy_) * analysis_alpha_energy;
    aperiodic_energy_ += (std::abs(input - analysis_low_) - aperiodic_energy_) * analysis_alpha_energy;
    const float tonal_balance = periodic_energy_ / std::max(0.0001f, periodic_energy_ + aperiodic_energy_);
    const float periodic_mix = confidence_mix * clamp((tonal_balance - 0.12f) / 0.74f, 0.0f, 1.0f);
    const float aperiodic_mix = 1.0f - periodic_mix;
    const VocalSourceType source_type = periodic_mix > 0.65f ? VocalSourceType::Periodic
        : (periodic_mix < 0.18f ? VocalSourceType::Aperiodic : VocalSourceType::Mixed);
    if (!voiced) {
        vocal_metrics_ = {tracked_hz, conf, correction_cents_, 0.0f, 0.0f, 0.0f, 1.0f, VocalSourceType::Aperiodic, false};
        return input;
    }
    float desired_cents = 0.0f;
    if (voiced) {
        const float input_cents = 1200.0f * std::log2(hz / 440.0f);
        const float fast_alpha = 1.0f - std::exp(-1.0f / (0.008f * sample_rate_));
        const float slow_alpha = 1.0f - std::exp(-1.0f / (0.110f * sample_rate_));
        pitch_fast_cents_ += (input_cents - pitch_fast_cents_) * fast_alpha;
        pitch_slow_cents_ += (pitch_fast_cents_ - pitch_slow_cents_) * slow_alpha;
        const float vibrato = pitch_fast_cents_ - pitch_slow_cents_;
        const float snapped = std::round(pitch_slow_cents_ / 100.0f) * 100.0f;
        const float retained_pitch = pitch_slow_cents_ + vibrato * clamp(c.vibrato_retention, 0.0f, 1.0f);
        desired_cents = (snapped - retained_pitch) * clamp(c.correction_strength, 0.0f, 1.0f) * (1.0f - clamp(c.drift_retention, 0.0f, 1.0f));
    }
    const float transition_ms = 2.0f + (1.0f - clamp(c.transition, 0.0f, 1.0f)) * 58.0f;
    const float correction_alpha = 1.0f - std::exp(-1.0f / (transition_ms * 0.001f * sample_rate_));
    correction_cents_ += (desired_cents - correction_cents_) * correction_alpha;

    // V1: two 50%-overlapped trailing grains form the primary causal,
    // pitch-synchronous PSOLA layer. No future sample is read and all state is fixed.
    const float period = clamp(sample_rate_ / hz, 53.0f, 872.0f);
    const float sequential_mix = clamp(c.sequential_stage_mix, 0.0f, 1.0f);
    const float primary_cents = correction_cents_ * (1.0f - 0.5f * sequential_mix);
    const float secondary_cents = correction_cents_ - primary_cents;
    const float primary_ratio = std::pow(2.0f, primary_cents / 1200.0f);
    grain_phase_ -= primary_ratio / period;
    if (grain_phase_ < 0.0f) grain_phase_ += 1.0f;
    const float phase_b = grain_phase_ < 0.5f ? grain_phase_ + 0.5f : grain_phase_ - 0.5f;
    const float base_lag = clamp(period * 1.15f, 64.0f, static_cast<float>(RING_BUFFER_SIZE - 900));
    const float wa = std::sin(PI * grain_phase_), wb = std::sin(PI * phase_b);
    const float psola_primary = ring_buffer_.read_interpolated(base_lag + grain_phase_ * period) * wa * wa + ring_buffer_.read_interpolated(base_lag + phase_b * period) * wb * wb;
    vocal_stage_ring_buffer_.push(psola_primary);

    // V3: a real optional second, softer causal PSOLA pass over the primary
    // history.  At zero it is a true bypass; toward one, correction is shared
    // approximately 50/50 between stages.  It is not called spectral mixing.
    secondary_grain_phase_ -= std::pow(2.0f, secondary_cents / 1200.0f) / period;
    if (secondary_grain_phase_ < 0.0f) secondary_grain_phase_ += 1.0f;
    const float phase_second_b = secondary_grain_phase_ < 0.5f ? secondary_grain_phase_ + 0.5f : secondary_grain_phase_ - 0.5f;
    const float ws0 = std::sin(PI * secondary_grain_phase_);
    const float ws1 = std::sin(PI * phase_second_b);
    const float psola_secondary = vocal_stage_ring_buffer_.read_interpolated(base_lag + secondary_grain_phase_ * period) * ws0 * ws0
        + vocal_stage_ring_buffer_.read_interpolated(base_lag + phase_second_b * period) * ws1 * ws1;
    const float psola = lerp(psola_primary, psola_secondary, sequential_mix);

    // One envelope/formant repair sits between the voiced layer and the bounded
    // aperiodic residual. It restores broad vocal energy, not a spectral clone.
    const float env_alpha = 1.0f - std::exp(-1.0f / (0.018f * sample_rate_));
    source_envelope_ += (std::abs(input) - source_envelope_) * env_alpha;
    shifted_envelope_ += (std::abs(psola) - shifted_envelope_) * env_alpha;
    const float repair = lerp(1.0f, clamp(source_envelope_ / std::max(0.015f, shifted_envelope_), 0.65f, 1.55f), clamp(c.formant_repair, 0.0f, 1.0f));
    const float repaired = psola * repair;
    residual_low_ += (input - residual_low_) * (1.0f - std::exp(-TWO_PI * 1700.0f / sample_rate_));
    const float residual_request = std::min(0.25f, clamp(c.spectral_residual_mix, 0.0f, 1.0f) * 0.25f) * (1.0f - periodic_mix * 0.35f);
    const float protected_aperiodic = aperiodic_mix * clamp(c.aperiodic_protection, 0.0f, 1.0f);
    const float transform_mix = periodic_mix * (1.0f - protected_aperiodic * 0.82f);
    // Treat the periodic transform, residual, and original component as a
    // bounded partition.  An aggressive creative setting cannot turn the dry
    // component negative or accidentally amplify the recombination.
    const float residual_mix = std::min(residual_request, 1.0f - transform_mix);
    const float dual_layer = repaired * transform_mix + (input - residual_low_) * residual_mix + input * (1.0f - transform_mix - residual_mix);
    const float colored = lerp(dual_layer, fast_tanh(dual_layer * (1.0f + clamp(c.character, 0.0f, 1.0f) * 2.5f)), clamp(c.character, 0.0f, 1.0f) * 0.35f);
    vocal_metrics_ = {tracked_hz, conf, correction_cents_, confidence_mix, residual_mix, periodic_mix, aperiodic_mix, source_type, voiced};
    return sanitize(lerp(input, colored, clamp(c.mix, 0.0f, 1.0f)));
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
