#include "monkeys_ear/drive_tube.h"
#include <cmath>

namespace monkeys_ear {
namespace {
inline float triode_transfer(float u) noexcept {
    return u>=0.0f ? fast_tanh(u) : 0.85f*fast_tanh(u*1.25f);
}
}

TubeDriveStage::TubeDriveStage()
    : sample_rate_(48000.0f),
      drive_(0.2f),
      bias_(0.1f),
      memory_sag_(0.2f),
      mix_(0.8f),
      cathode_charge_(0.0f),
      sag_coeff_(0.0f) {
    recalculate();
}

void TubeDriveStage::set_sample_rate(float sr) {
    sample_rate_ = std::max(1000.0f, sr);
    recalculate();
}

void TubeDriveStage::set_drive(float drive) {
    drive_ = clamp(drive, 0.0f, 1.0f);
    warmth_coeff_=lerp(warmth_open_coeff_,warmth_hot_coeff_,drive_);
}

void TubeDriveStage::set_bias(float bias) {
    bias_ = clamp(bias, -1.0f, 1.0f);
}

void TubeDriveStage::set_memory_sag(float sag) {
    memory_sag_ = clamp(sag, 0.0f, 1.0f);
}

void TubeDriveStage::set_mix(float mix) {
    mix_ = clamp(mix, 0.0f, 1.0f);
}

void TubeDriveStage::reset() {
    cathode_charge_ = 0.0f;
    warmth_state_=0.0f;
}

void TubeDriveStage::recalculate() {
    // Cathode sag time constant ~30ms
    float time_constant_s = 0.030f;
    sag_coeff_ = std::exp(-1.0f / (time_constant_s * sample_rate_));
    // The strong-excitation response retains body and even harmonics while
    // attenuating the topmost grit. Like the rack's existing bandwidth model,
    // both coefficients are prepared at the sample-rate boundary.
    const float open_hz=std::min(18000.0f,sample_rate_*0.43f);
    const float hot_hz=std::min(10000.0f,sample_rate_*0.43f);
    warmth_open_coeff_=1.0f-std::exp(-TWO_PI*open_hz/sample_rate_);
    warmth_hot_coeff_=1.0f-std::exp(-TWO_PI*hot_hz/sample_rate_);
    warmth_coeff_=lerp(warmth_open_coeff_,warmth_hot_coeff_,drive_);
}

float TubeDriveStage::process(float input) {
    if (drive_ < 0.001f && mix_ < 0.001f) {
        return input;
    }

    // Input gain scaling: 1x to 25x (+0dB to +28dB)
    float gain = 1.0f + drive_ * 24.0f;
    float x = input * gain;

    // Causal dynamic cathode memory sag tracker:
    // Tracks rectified signal energy to simulate cathode bias shift under heavy drive
    float abs_x = std::abs(x);
    cathode_charge_ = abs_x * (1.0f - sag_coeff_) + cathode_charge_ * sag_coeff_;

    // Dynamic bias calculation
    float dynamic_bias = bias_ * 0.4f - (cathode_charge_ * memory_sag_ * 0.15f);

    // A biased transfer curve must be measured relative to its own idle
    // operating point. Without this subtraction a silent synth emits DC,
    // which eats headroom and biases downstream nonlinear modules.
    const float saturated=triode_transfer(x+dynamic_bias)-triode_transfer(dynamic_bias);

    // Output level compensation so extreme drive maintains musical volume
    float makeup_gain = 1.0f / std::sqrt(1.0f + drive_ * 8.0f);
    float wet=saturated*makeup_gain;
    warmth_state_+=(wet-warmth_state_)*warmth_coeff_;
    // Wet-only bandwidth preserves the underlying clean fundamental when
    // the musician lowers Heat. No extra path or reverb-like ringout.
    float out=lerp(input,warmth_state_,mix_);
    return sanitize(out);
}

} // namespace monkeys_ear
