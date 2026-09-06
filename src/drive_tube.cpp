#include "monkeys_ear/drive_tube.h"
#include <cmath>

namespace monkeys_ear {

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
}

void TubeDriveStage::recalculate() {
    // Cathode sag time constant ~30ms
    float time_constant_s = 0.030f;
    sag_coeff_ = std::exp(-1.0f / (time_constant_s * sample_rate_));
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

    // Asymmetric triode-style nonlinear waveshaping function:
    // f(u) = tanh(u + dynamic_bias) + 0.15 * (1.0 - exp(-|u|)) * sign(u)
    float u = x + dynamic_bias;
    float saturated = 0.0f;

    if (u >= 0.0f) {
        // Positive half-cycle: soft compression
        saturated = fast_tanh(u);
    } else {
        // Negative half-cycle: harder clipping with quadratic onset (triode grid current)
        float u_neg = u * 1.25f;
        saturated = fast_tanh(u_neg) * 0.85f;
    }

    // Output level compensation so extreme drive maintains musical volume
    float makeup_gain = 1.0f / std::sqrt(1.0f + drive_ * 8.0f);
    float wet = saturated * makeup_gain;

    // Crossfade dry/wet
    float out = lerp(input, wet, mix_);
    return sanitize(out);
}

} // namespace monkeys_ear
