#include "monkeys_ear/vocal_module.h"
#include <algorithm>
#include <cmath>

namespace monkeys_ear {

const ModuleIdentity VocalModule::kIdentity{
    "monkeys-ear.vocal", "Monkey's Ear Vocal", "Fx|Pitch Shift",
    ModuleKind::AudioEffect, 1u
};

VocalModule::VocalModule() noexcept {
    controls_.enabled = true;
    controls_.correction_strength = 0.45f;
    controls_.drift_retention = 0.35f;
    controls_.vibrato_retention = 0.80f;
    controls_.transition = 0.55f;
    controls_.formant_repair = 0.50f;
    controls_.spectral_residual_mix = 0.10f;
    controls_.character = 0.0f;
    controls_.mix = 1.0f;
    controls_.sequential_stage_mix = 0.35f;
    controls_.aperiodic_protection = 0.90f;
    processor_.set_vocal_controls(controls_);
}

const ModuleIdentity& VocalModule::identity() const noexcept { return kIdentity; }
ModuleIOContract VocalModule::io_contract() const noexcept {
    ModuleIOContract io{};
    io.audio_inputs = 2; io.audio_outputs = 2;
    io.accepts_midi = false;
    io.produces_midi = false; io.reports_zero_latency = true;
    return io;
}

void VocalModule::prepare(float sample_rate, uint32_t max_block_size) noexcept {
    sample_rate = sanitize(sample_rate);
    sample_rate_ = sample_rate >= 1000.0f ? clamp(sample_rate, 1000.0f, 768000.0f) : 48000.0f;
    max_block_size_ = std::max<uint32_t>(1u, std::min<uint32_t>(max_block_size, 65536u));
    processor_.set_sample_rate(sample_rate_);
    processor_.set_gain(0.0f); processor_.set_mix(1.0f);
    processor_.set_highpass_enabled(false);
    processor_.set_vocal_controls(controls_); processor_.reset();
    health_ = {true, true, true};
}

void VocalModule::reset() noexcept { processor_.reset(); health_.finite_output = true; }
void VocalModule::set_controls(const VocalExpressionControls& controls) noexcept {
    controls_ = controls; controls_.enabled = true;
    processor_.set_vocal_controls(controls_);
}

void VocalModule::process_block(const float* input_l, const float* input_r,
                                float* output_l, float* output_r, uint32_t num_samples,
                                float tracked_hz, float confidence) noexcept {
    if (!output_l || !output_r) { health_.state_valid = false; return; }
    const uint32_t n = std::min(num_samples, max_block_size_);
    for (uint32_t i = 0; i < n; ++i) {
        const float l = input_l ? sanitize(input_l[i]) : 0.0f;
        const float r = input_r ? sanitize(input_r[i]) : l;
        if (bypass_) { output_l[i] = l; output_r[i] = r; continue; }
        const float mid = 0.5f * (l + r), side = 0.5f * (l - r);
        const float conditioned = processor_.process_sample(mid, 0.0f);
        const float wet_mid = processor_.process_vocal_sample(conditioned, tracked_hz, confidence);
        float out_l = sanitize(wet_mid + side), out_r = sanitize(wet_mid - side);
        if (!std::isfinite(out_l) || !std::isfinite(out_r)) {
            out_l = l; out_r = r; health_.finite_output = false;
        }
        output_l[i] = out_l; output_r[i] = out_r;
    }
    for (uint32_t i = n; i < num_samples; ++i) {
        output_l[i] = input_l ? sanitize(input_l[i]) : 0.0f;
        output_r[i] = input_r ? sanitize(input_r[i]) : output_l[i];
    }
}

} // namespace monkeys_ear
