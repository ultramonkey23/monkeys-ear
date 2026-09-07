#include "monkeys_ear/lfo.h"
#include <cmath>
#include <algorithm>

namespace monkeys_ear {

LFO::LFO()
    : sample_rate_(48000.0f),
      rate_hz_(1.5f),
      depth_(0.0f),
      waveform_(LFOWaveform::Sine),
      phase_(0.0f),
      phase_increment_(0.0f),
      current_value_(0.0f),
      last_sh_val_(0.0f),
      rng_state_(0xDEADBEEF) {
    update_increment();
}

void LFO::set_sample_rate(float sr) {
    sample_rate_ = std::max(1000.0f, sr);
    update_increment();
}

void LFO::set_rate_hz(float rate_hz) {
    rate_hz_ = clamp(rate_hz, 0.05f, 30.0f);
    update_increment();
}

void LFO::set_depth(float depth) {
    depth_ = clamp(depth, 0.0f, 1.0f);
}

void LFO::set_waveform(LFOWaveform wf) {
    waveform_ = wf;
}

void LFO::reset_phase(float phase) {
    phase_ = std::fmod(phase, 1.0f);
    if (phase_ < 0.0f) phase_ += 1.0f;
}

void LFO::update_increment() {
    phase_increment_ = rate_hz_ / sample_rate_;
}

float LFO::next_random() {
    rng_state_ = rng_state_ * 1664525u + 1013904223u;
    return (static_cast<float>(rng_state_) / 4294967296.0f) * 2.0f - 1.0f;
}

float LFO::process() {
    float raw = 0.0f;

    switch (waveform_) {
        case LFOWaveform::Sine:
            raw = std::sin(phase_ * TWO_PI);
            break;

        case LFOWaveform::Triangle:
            raw = 1.0f - 4.0f * std::abs(std::round(phase_ - 0.25f) - (phase_ - 0.25f));
            break;

        case LFOWaveform::Saw:
            raw = 2.0f * phase_ - 1.0f;
            break;

        case LFOWaveform::Square:
            raw = (phase_ < 0.5f) ? 1.0f : -1.0f;
            break;

        case LFOWaveform::SampleAndHold:
            raw = last_sh_val_;
            break;
    }

    phase_ += phase_increment_;
    if (phase_ >= 1.0f) {
        phase_ -= 1.0f;
        if (waveform_ == LFOWaveform::SampleAndHold) {
            last_sh_val_ = next_random();
        }
    }

    current_value_ = raw * depth_;
    return current_value_;
}

} // namespace monkeys_ear
