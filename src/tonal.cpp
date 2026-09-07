#include "monkeys_ear/tonal.h"
#include <cmath>

namespace monkeys_ear {

void MultiPassFilter::set_sample_rate(float sr) { for (auto& f : filters_) f.set_sample_rate(sr); }
void MultiPassFilter::reset() { for (auto& f : filters_) f.reset(); }

void MultiPassFilter::set_stage(int stage, FilterMode mode, float cutoff_hz,
                                float resonance, float drive, bool slope_24db) {
    if (stage < 0 || stage > 1) return;
    slope_24db_[stage] = slope_24db;
    for (int copy = 0; copy < 2; ++copy) {
        auto& f = filters_[stage * 2 + copy];
        f.set_mode(mode);
        f.set_cutoff(cutoff_hz);
        f.set_resonance(resonance);
        f.set_drive(drive);
    }
}

float MultiPassFilter::process(float input, float modulation, float modulation_depth) {
    auto stage = [&](int index, float x) {
        float y = filters_[index * 2].process(x, modulation, modulation_depth);
        return slope_24db_[index]
            ? filters_[index * 2 + 1].process(y, modulation, modulation_depth)
            : y;
    };
    float wet = input;
    if (routing_ == FilterRouting::Serial) {
        wet = stage(1, stage(0, input));
    } else if (routing_ == FilterRouting::Parallel) {
        wet = 0.5f * (stage(0, input) + stage(1, input));
    } else {
        // Split keeps the low branch authoritative and shapes the complementary
        // upper branch independently; recombination is bounded to unity gain.
        wet = 0.65f * stage(0, input) + 0.35f * stage(1, input);
    }
    return sanitize(lerp(input, wet, mix_));
}

void ParametricEQ::set_sample_rate(float sr) {
    sample_rate_ = std::max(1000.0f, sr);
    for (size_t i = 0; i < NUM_BANDS; ++i) update_band(i, 0.0f, 0.0f);
}

void ParametricEQ::reset() {
    for (auto& b : bands_) { b.z1 = 0.0f; b.z2 = 0.0f; b.current = b.target; }
    compensation_current_ = 1.0f;
}

void ParametricEQ::set_band(size_t band, const EQBandSettings& settings) {
    if (band >= NUM_BANDS) return;
    settings_[band] = settings;
    settings_[band].frequency_hz = clamp(settings.frequency_hz, 20.0f, sample_rate_ * 0.45f);
    settings_[band].gain_db = clamp(settings.gain_db, -18.0f, 18.0f);
    settings_[band].q = clamp(settings.q, 0.15f, 12.0f);
    update_band(band, 0.0f, 0.0f);
}

void ParametricEQ::update_band(size_t band, float frequency_mod_octaves, float gain_mod_db) {
    const auto& s = settings_[band];
    float f = clamp(s.frequency_hz * std::pow(2.0f, frequency_mod_octaves), 20.0f, sample_rate_ * 0.45f);
    float gain = clamp(s.gain_db + gain_mod_db, -18.0f, 18.0f);
    float A = std::pow(10.0f, gain / 40.0f);
    float w0 = TWO_PI * f / sample_rate_;
    float c = std::cos(w0), sn = std::sin(w0);
    float alpha = sn / (2.0f * s.q);
    float b0=1, b1=0, b2=0, a0=1, a1=0, a2=0;
    switch (s.type) {
        case EQType::Bell:
            b0=1+alpha*A; b1=-2*c; b2=1-alpha*A; a0=1+alpha/A; a1=-2*c; a2=1-alpha/A; break;
        case EQType::LowShelf: {
            float two = 2.0f * std::sqrt(A) * alpha;
            b0=A*((A+1)-(A-1)*c+two); b1=2*A*((A-1)-(A+1)*c); b2=A*((A+1)-(A-1)*c-two);
            a0=(A+1)+(A-1)*c+two; a1=-2*((A-1)+(A+1)*c); a2=(A+1)+(A-1)*c-two; break;
        }
        case EQType::HighShelf: {
            float two = 2.0f * std::sqrt(A) * alpha;
            b0=A*((A+1)+(A-1)*c+two); b1=-2*A*((A-1)+(A+1)*c); b2=A*((A+1)+(A-1)*c-two);
            a0=(A+1)-(A-1)*c+two; a1=2*((A-1)-(A+1)*c); a2=(A+1)-(A-1)*c-two; break;
        }
        case EQType::Highpass:
            b0=(1+c)*0.5f; b1=-(1+c); b2=(1+c)*0.5f; a0=1+alpha; a1=-2*c; a2=1-alpha; break;
        case EQType::Lowpass:
            b0=(1-c)*0.5f; b1=1-c; b2=(1-c)*0.5f; a0=1+alpha; a1=-2*c; a2=1-alpha; break;
        case EQType::Bypass: break;
    }
    bands_[band].target = {b0/a0, b1/a0, b2/a0, a1/a0, a2/a0};
}

float ParametricEQ::process(float input, float frequency_mod_octaves, float gain_mod_db) {
    if (bypass_) return input;
    float x = input;
    float positive_gain = 0.0f;
    for (size_t i = 0; i < NUM_BANDS; ++i) {
        if (settings_[i].type == EQType::Bypass) continue;
        if ((modulation_counter_ & 15u) == 0u)
            update_band(i, (i == 1 ? frequency_mod_octaves : 0.0f), (i == 1 ? gain_mod_db : 0.0f));
        auto& b = bands_[i];
        constexpr float smoothing = 0.0025f;
        b.current.b0 += (b.target.b0-b.current.b0)*smoothing;
        b.current.b1 += (b.target.b1-b.current.b1)*smoothing;
        b.current.b2 += (b.target.b2-b.current.b2)*smoothing;
        b.current.a1 += (b.target.a1-b.current.a1)*smoothing;
        b.current.a2 += (b.target.a2-b.current.a2)*smoothing;
        float y = b.current.b0*x + b.z1;
        b.z1 = b.current.b1*x - b.current.a1*y + b.z2;
        b.z2 = b.current.b2*x - b.current.a2*y;
        x = sanitize(y);
        if (settings_[i].gain_db > 0.0f) positive_gain += settings_[i].gain_db;
    }
    float target_comp = gain_compensation_ ? std::pow(10.0f, -positive_gain * 0.20f / 20.0f) : 1.0f;
    compensation_current_ += (target_comp - compensation_current_) * 0.0005f;
    ++modulation_counter_;
    return sanitize(x * compensation_current_);
}

void ExternalSubharmonic::set_sample_rate(float sr) { sample_rate_ = std::max(1000.0f, sr); reset(); }
void ExternalSubharmonic::reset() {
    previous_input_=0; samples_since_crossing_=0; previous_period_=0; stale_samples_=0;
    phase_=0; tracked_frequency_hz_=0; confidence_=0; envelope_=0;
}

float ExternalSubharmonic::process(float input) {
    float abs_in = std::abs(input);
    float env_coeff = abs_in > envelope_ ? 0.02f : 0.0008f;
    envelope_ += (abs_in - envelope_) * env_coeff;
    ++samples_since_crossing_;
    ++stale_samples_;
    if (previous_input_ <= 0.0f && input > 0.0f && envelope_ > 0.008f) {
        int period = samples_since_crossing_;
        float candidate = sample_rate_ / static_cast<float>(std::max(1, period));
        if (candidate >= 45.0f && candidate <= 500.0f) {
            float stability = previous_period_ > 0
                ? 1.0f - std::abs(static_cast<float>(period-previous_period_)) / static_cast<float>(previous_period_)
                : 0.0f;
            confidence_ += (clamp(stability, 0.0f, 1.0f) - confidence_) * 0.35f;
            tracked_frequency_hz_ += (candidate - tracked_frequency_hz_) * 0.25f;
            previous_period_ = period;
            stale_samples_ = 0;
        } else confidence_ *= 0.8f;
        samples_since_crossing_ = 0;
    }
    previous_input_ = input;
    if (stale_samples_ > static_cast<int>(sample_rate_ * 0.05f)) confidence_ *= 0.999f;
    if (tracked_frequency_hz_ > 0.0f) {
        phase_ += tracked_frequency_hz_ / (sample_rate_ * static_cast<float>(ratio_denominator_));
        if (phase_ >= 1.0f) phase_ -= std::floor(phase_);
    }
    float generated = std::sin(TWO_PI * (phase_ + phase_offset_)) * polarity_;
    float level = envelope_ * clamp((confidence_ - 0.35f) * 1.54f, 0.0f, 1.0f);
    float out = generated * level;
    if (saturation_ > 0.0f) out = lerp(out, fast_tanh(out * (1.0f + saturation_ * 5.0f)), saturation_);
    return sanitize(out);
}

void MotionSmoother::set_sample_rate(float sr, float time_ms) {
    coefficient_ = std::exp(-1.0f / (std::max(0.1f, time_ms) * 0.001f * std::max(1000.0f, sr)));
}
float MotionSmoother::process(float target) {
    value_ = coefficient_ * value_ + (1.0f - coefficient_) * target;
    return sanitize(value_);
}

} // namespace monkeys_ear
