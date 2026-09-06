#include "monkeys_ear/filter.h"
#include <cmath>

namespace monkeys_ear {

StateVariableFilter::StateVariableFilter()
    : sample_rate_(48000.0f),
      cutoff_hz_(1500.0f),
      resonance_(0.2f),
      mode_(FilterMode::Lowpass),
      drive_(0.0f),
      s1_(0.0f),
      s2_(0.0f),
      g_(0.0f),
      k_(0.0f) {
    update_coefficients();
}

void StateVariableFilter::set_sample_rate(float sr) {
    sample_rate_ = std::max(1000.0f, sr);
    update_coefficients();
}

void StateVariableFilter::set_cutoff(float freq_hz) {
    cutoff_hz_ = clamp(freq_hz, 20.0f, sample_rate_ * 0.48f);
    update_coefficients();
}

void StateVariableFilter::set_resonance(float res) {
    resonance_ = clamp(res, 0.0f, 0.98f);
    update_coefficients();
}

void StateVariableFilter::set_mode(FilterMode mode) {
    mode_ = mode;
}

void StateVariableFilter::set_drive(float drive) {
    drive_ = clamp(drive, 0.0f, 2.0f);
}

void StateVariableFilter::reset() {
    s1_ = 0.0f;
    s2_ = 0.0f;
}

void StateVariableFilter::update_coefficients() {
    // Trapezoidal integration warped frequency: g = tan(pi * fc / fs)
    float wd = TWO_PI * cutoff_hz_;
    float wa = (2.0f * sample_rate_) * std::tan(wd / (2.0f * sample_rate_));
    g_ = wa / (2.0f * sample_rate_);

    // Resonance damping factor: k = 2.0 - 2.0 * res (res=0 -> k=2, res=1 -> k=0)
    k_ = 2.0f * (1.0f - resonance_);
}

float StateVariableFilter::process(float input, float env_mod, float env_amount) {
    // Dynamic cutoff calculation with envelope modulation
    float effective_cutoff = cutoff_hz_;
    if (std::abs(env_amount) > 1e-4f) {
        // Modulate in octaves: +/- 5 octaves
        float octave_shift = env_mod * env_amount * 5.0f;
        effective_cutoff *= std::pow(2.0f, octave_shift);
        effective_cutoff = clamp(effective_cutoff, 20.0f, sample_rate_ * 0.48f);
    }

    // Dynamic g calculation if cutoff is modulated
    float g = g_;
    if (std::abs(effective_cutoff - cutoff_hz_) > 1.0f) {
        float wd = TWO_PI * effective_cutoff;
        float wa = (2.0f * sample_rate_) * std::tan(wd / (2.0f * sample_rate_));
        g = wa / (2.0f * sample_rate_);
    }

    // Input saturation / drive
    float driven_input = input;
    if (drive_ > 1e-4f) {
        driven_input = fast_tanh(input * (1.0f + drive_ * 2.0f));
    }

    // Cytomic ZDF SVF solution:
    // Solve loop: hp = (in - k * s1 - g * s1 - s2) / (1 + g * (g + k))
    float denom = 1.0f / (1.0f + g * (g + k_));
    float hp = (driven_input - k_ * s1_ - g * s1_ - s2_) * denom;

    // First integrator: bandpass output
    float bp = g * hp + s1_;
    // Non-linear feedback saturation prevents runaway self-oscillation
    bp = fast_tanh(bp);
    s1_ = g * hp + bp;

    // Second integrator: lowpass output
    float lp = g * bp + s2_;
    s2_ = g * bp + lp;

    // Notch
    float notch = hp + lp;

    float out = 0.0f;
    switch (mode_) {
        case FilterMode::Lowpass:
            out = lp;
            break;
        case FilterMode::Bandpass:
            out = bp;
            break;
        case FilterMode::Highpass:
            out = hp;
            break;
        case FilterMode::Notch:
            out = notch;
            break;
    }

    return sanitize(out);
}

} // namespace monkeys_ear
