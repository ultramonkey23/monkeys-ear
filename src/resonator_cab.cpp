#include "monkeys_ear/resonator_cab.h"
#include <cmath>

namespace monkeys_ear {

CabinetResonator::CabinetResonator()
    : sample_rate_(48000.0f),
      body_size_(1.0f),
      resonance_(0.3f),
      damping_(0.4f),
      mix_(0.4f) {
    update_poles();
}

void CabinetResonator::set_sample_rate(float sr) {
    sample_rate_ = std::max(1000.0f, sr);
    update_poles();
}

void CabinetResonator::set_body_size(float size) {
    body_size_ = clamp(size, 0.4f, 2.5f);
    update_poles();
}

void CabinetResonator::set_resonance(float res) {
    resonance_ = clamp(res, 0.0f, 0.95f);
    update_poles();
}

void CabinetResonator::set_damping(float damping) {
    damping_ = clamp(damping, 0.0f, 1.0f);
    update_poles();
}

void CabinetResonator::set_mix(float mix) {
    mix_ = clamp(mix, 0.0f, 1.0f);
}

void CabinetResonator::reset() {
    for (auto& pole : poles_) {
        pole.reset();
    }
}

void CabinetResonator::update_poles() {
    // 4 characteristic cabinet body modes (tuned to authentic acoustic guitar/cabinet resonances):
    // Mode 1: Main Helmholtz air cavity (~110 Hz scaled by body size)
    // Mode 2: Back plate / main wood resonance (~220 Hz)
    // Mode 3: Top plate cross dipole (~460 Hz)
    // Mode 4: Upper body / cone breakup (~1150 Hz)
    float base_freqs[4] = {110.0f, 220.0f, 460.0f, 1150.0f};
    float base_gains[4] = {0.8f, 0.6f, 0.5f, 0.35f};

    float decay_base = 0.015f + resonance_ * 0.12f; // 15ms to 135ms decay

    for (size_t i = 0; i < 4; ++i) {
        float f = base_freqs[i] / body_size_;
        f = clamp(f, 20.0f, sample_rate_ * 0.45f);

        // High frequencies damp faster
        float damp_factor = 1.0f / (1.0f + damping_ * (static_cast<float>(i) * 0.75f));
        float decay = decay_base * damp_factor;

        poles_[i].freq_hz = f;
        poles_[i].decay_s = decay;
        poles_[i].gain = base_gains[i];
        poles_[i].update(sample_rate_);
    }
}

float CabinetResonator::process(float input) {
    if (mix_ < 0.001f) {
        return input;
    }

    float resonant_body = 0.0f;
    for (auto& pole : poles_) {
        resonant_body += pole.process(input) * pole.gain;
    }

    // Blend dry and resonant body
    float out = lerp(input, resonant_body, mix_);
    return sanitize(out);
}

} // namespace monkeys_ear
